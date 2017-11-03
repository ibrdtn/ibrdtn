/*
 * DataStorage.h
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

#include <iostream>
#include <fstream>
#include <string>
#include <ibrdtn/data/BundleID.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Semaphore.h>
#include <memory>

#ifndef DATASTORAGE_H_
#define DATASTORAGE_H_

namespace dtn
{
	namespace storage
	{
		class DataStorage : public ibrcommon::JoinableThread
		{
		public:
			class DataNotAvailableException : public ibrcommon::Exception
			{
			public:
				DataNotAvailableException(std::string what = "Requested data is not available.") throw() : Exception(what)
				{ };
			};

			class Container
			{
			public:
				virtual ~Container() = 0;
				virtual std::string getId() const = 0;
				virtual std::ostream& serialize(std::ostream &stream) = 0;
			};

			class Hash
			{
			public:
				Hash();
				Hash(const std::string &value);
				Hash(const DataStorage::Container &container);
				Hash(const ibrcommon::File &file);
				virtual ~Hash();

				bool operator!=(const Hash &other) const;
				bool operator==(const Hash &other) const;
				bool operator<(const Hash &other) const;

				std::string value;
			};

			class istream : public ibrcommon::File
			{
			public:
				istream(ibrcommon::Mutex &mutex, const ibrcommon::File &file);
				virtual ~istream();
				std::istream& operator*();

			private:
				std::ifstream *_stream;
				ibrcommon::Mutex &_lock;
			};

			class Callback
			{
			public:
				virtual ~Callback() { };
				virtual void eventDataStorageStored(const Hash &hash) = 0;
				virtual void eventDataStorageStoreFailed(const Hash &hash, const ibrcommon::Exception&) = 0;
				virtual void eventDataStorageRemoved(const Hash &hash) = 0;
				virtual void eventDataStorageRemoveFailed(const Hash &hash, const ibrcommon::Exception&) = 0;
				virtual void iterateDataStorage(const Hash &hash, DataStorage::istream &stream) = 0;
			};

			DataStorage(Callback &callback, const ibrcommon::File &path, unsigned int write_buffer = 0, bool initialize = false);
			virtual ~DataStorage();

			const Hash store(Container *data);
			void store(const Hash &hash, Container *data);

			DataStorage::istream retrieve(const Hash &hash) throw (DataNotAvailableException);
			void remove(const Hash &hash);

			/**
			 * wait until all tasks are completed
			 */
			void wait();

			/**
			 * iterate through all the data and call the iterateDataStorage() on each dataset
			 */
			void iterateAll();

			/**
			 * reset the data storage
			 */
			void reset();

			/*** BEGIN: methods for unit-testing ***/

			/**
			 * Set the storage to faulty. If set to true, each try to store
			 * a bundle will fail.
			 */
			void setFaulty(bool mode);

			/*** END: methods for unit-testing ***/

		protected:
			void run() throw ();
			void __cancellation() throw ();

		private:
			class Task
			{
			public:
				virtual ~Task() = 0;
			};

			class StoreDataTask : public Task
			{
			public:
				StoreDataTask(const Hash &h, Container *c);
				virtual ~StoreDataTask();

				const Hash hash;
				std::auto_ptr<Container> _container;
			};

			class RemoveDataTask : public Task
			{
			public:
				RemoveDataTask(const Hash &h);
				virtual ~RemoveDataTask();

				const Hash hash;
			};

			Callback &_callback;
			ibrcommon::File _path;
			ibrcommon::Queue< Task* > _tasks;
			ibrcommon::Semaphore _store_sem;
			bool _store_limited;
			bool _faulty;

			ibrcommon::Mutex _global_mutex;
		};
	}
}

#endif /* DATASTORAGE_H_ */
