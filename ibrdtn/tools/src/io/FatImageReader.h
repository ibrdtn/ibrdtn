/*
 * FatImageReader.h
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
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
 *  Created on: Mar 14, 2014
 */

#ifndef FATIMAGEREADER_H_
#define FATIMAGEREADER_H_

#include "io/FATFile.h"
#include <ibrcommon/data/File.h>
#include <list>

extern "C" //libtffs does not support c++
{
#include <tffs.h>
}

namespace io
{
	class FatImageReader
	{
	public:
		class FatImageException : public ibrcommon::IOException
		{
		public:
			FatImageException(int errcode, const std::string &operation, const ibrcommon::File &file);
			virtual ~FatImageException() noexcept;

			int getErrorCode() const;

		private:
			static std::string create_message(int errcode, const std::string &operation, const ibrcommon::File &file);
			const int _errorcode;
		};

		class FileHandle
		{
		public:
			FileHandle(const ibrcommon::File &image, const std::string &path);
			virtual ~FileHandle();

			size_t read(unsigned char *buffer, size_t buf_size);

			void close();

		private:
			tdir_handle_t _hdir;
			tffs_handle_t _htffs;
			tfile_handle_t _hfile;
			bool _open;
		};

		FatImageReader(const ibrcommon::File &filename);
		virtual ~FatImageReader();

		typedef std::list<FATFile> filelist;

		void list(filelist &files) const noexcept (false);
		void list(const FATFile &directory, filelist &files) const noexcept (false);

		bool exists(const FATFile &file) const noexcept;

		time_t lastaccess(const FATFile &file) const noexcept;

		size_t size(const FATFile &file) const noexcept;

		bool isDirectory(const FATFile &file) const noexcept;

		FileHandle open(const FATFile &file) const;

	private:
		void update(const FATFile &path, dirent_t &d) const noexcept (false);

		const static std::string TAG;

		const ibrcommon::File _filename;
	};
} /* namespace io */

#endif /* FATIMAGEREADER_H_ */
