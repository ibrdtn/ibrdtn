/*
 * BLOB.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef BLOB_H_
#define BLOB_H_

#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/data/File.h"
#include "ibrcommon/thread/Semaphore.h"
#include "ibrcommon/refcnt_ptr.h"
#include <iostream>
#include <sstream>
#include <fstream>

namespace ibrcommon
{
	class CanNotOpenFileException : public ibrcommon::IOException
	{
	public:
		CanNotOpenFileException(ibrcommon::File f) noexcept : IOException("Could not open file " + f.getPath() + ".")
		{
		};
	};

	class BLOB : public Mutex
	{
	public:
		/**
		 * This is the global limit for open file handles in BLOBs
		 */
		static ibrcommon::Semaphore _filelimit;

		/**
		 * copy a stream to another stream
		 */
		static std::ostream& copy(std::ostream &output, std::istream &input, const std::streamsize size, const size_t buffer_size = 0x1000);

		virtual ~BLOB();

		/**
		 * This method deletes the content of the payload.
		 * The size will be zero after calling.
		 */
		virtual void clear() = 0;

		virtual void open() = 0;
		virtual void close() = 0;

		std::streamsize size() const;

		// updates the const size of the BLOB
		void update();

		class iostream
		{
		private:
			BLOB &_blob;
			ibrcommon::MutexLock lock;
			std::iostream &_stream;

		public:
			iostream(BLOB &blob)
			 : _blob(blob), lock(blob), _stream(blob.__get_stream())
			{
				_blob.open();
			}

			virtual ~iostream()
			{
				_blob.close();
				_blob.update();
			};

			std::iostream* operator->() { return &(_blob.__get_stream()); };
			std::iostream& operator*() { return _blob.__get_stream(); };

			std::streamsize size()
			{
				return _blob.__get_size();
			};

			void clear()
			{
				_blob.clear();
			}
		};

		class Reference
		{
		public:
			Reference(BLOB *blob);
			Reference(const Reference &ref);
			virtual ~Reference();

			/**
			 * Get a direct access reference to the internal stream object.
			 * @return iostream reference.
			 */
			BLOB::iostream iostream();

			/**
			 * Get the pointer to the origin BLOB object.
			 * @return The pointer to the origin BLOB object
			 */
			const BLOB& operator*() const;

			/**
			 * Return the size of the BLOB data
			 */
			std::streamsize size() const;

		private:
			refcnt_ptr<BLOB> _blob;
		};

		/**
		 * This is the interface for all BLOB provider implementations.
		 * A BLOB provider manages large amounts of data which could be
		 * referenced several times in other objects using
		 * BLOB::Reference objects.
		 */
		class Provider
		{
		public:
			/**
			 * Destructor of the BLOB Provider
			 */
			virtual ~Provider() = 0;

			/**
			 * creates a BLOB reference
			 * @return Return a reference to a BLOB object.
			 */
			virtual BLOB::Reference create() = 0;
		};

		/**
		 * Create a new BLOB object.
		 * @return
		 */
		static ibrcommon::BLOB::Reference create();

		/**
		 * Open a file as read-only BLOB object.
		 * @return
		 */
		static ibrcommon::BLOB::Reference open(const ibrcommon::File &f);

		/**
		 * Changes the BLOB provider.
		 */
		static void changeProvider(BLOB::Provider *p, bool auto_delete = false);

	protected:
		class ProviderRef
		{
		public:
			ProviderRef(Provider *provider, bool auto_delete);
			virtual ~ProviderRef();

			void change(Provider *p, bool auto_delete = true);

			BLOB::Reference create();

		private:
			Provider *_provider;
			bool _auto_delete;
		};

		/**
		 * This is the global BLOB provider for all BLOB objects.
		 */
		static ProviderRef provider;

		BLOB(const std::streamsize intitial_size = 0);

		virtual std::streamsize __get_size() = 0;
		virtual std::iostream &__get_stream() = 0;

	private:
		BLOB(const BLOB &ref); // forbidden copy constructor
		std::streamsize _const_size;
	};

	/**
	 * A FileBLOB is a read only BLOB object. It is based on a fstream object, but
	 * denies write access. This could be used to easy access static files.
	 */
	class FileBLOB : public ibrcommon::BLOB
	{
	public:
		FileBLOB(const ibrcommon::File &f);
		virtual ~FileBLOB();

		virtual void clear();

		virtual void open();
		virtual void close();

	protected:
		std::iostream &__get_stream()
		{
			return _filestream;
		}

		std::streamsize __get_size();

	private:
		std::fstream _filestream;
		File _file;
	};

	class MemoryBLOBProvider : public ibrcommon::BLOB::Provider
	{
	public:
		MemoryBLOBProvider();
		virtual ~MemoryBLOBProvider();
		BLOB::Reference create();

	private:
		/**
		 * A StringBLOB contains a small amount of data, is based on a stringstream object
		 * and hence it is always held in memory.
		 */
		class StringBLOB : public BLOB
		{
		public:
			static BLOB::Reference create();
			virtual ~StringBLOB();

			virtual void clear();

			virtual void open();
			virtual void close();

		protected:
			std::iostream &__get_stream()
			{
				return _stringstream;
			}

			std::streamsize __get_size();

		private:
			StringBLOB();
			std::stringstream _stringstream;
		};
	};

	class FileBLOBProvider : public ibrcommon::BLOB::Provider
	{
	public:
		FileBLOBProvider(const File &path);
		virtual ~FileBLOBProvider();
		BLOB::Reference create();

	private:
		ibrcommon::File _tmppath;

		/**
		 * A TmpFileBLOB creates a temporary file on instantiation to hold a large amount
		 * of data. It is based on fstream objects and limited by the system architecture
		 * (max. 2GB on 32-bit systems).
		 */
		class TmpFileBLOB : public BLOB
		{
		public:
			TmpFileBLOB(const File &path);
			virtual ~TmpFileBLOB();

			virtual void clear();

			virtual void open();
			virtual void close();

		protected:
			std::iostream &__get_stream()
			{
				return _filestream;
			}

			std::streamsize __get_size();

		private:
			std::fstream _filestream;
			int _fd;
			TemporaryFile _tmpfile;
		};
	};
}

#endif /* BLOB_H_ */
