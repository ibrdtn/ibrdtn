/*
 * SQLiteBundleStorage.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 * Written-by: Matthias Myrtus
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


#ifndef SQLITEBUNDLESTORAGE_H_
#define SQLITEBUNDLESTORAGE_H_

#include "storage/BundleStorage.h"
#include "storage/SQLiteDatabase.h"
#include "storage/SQLiteBundleSet.h"

#include "Component.h"
#include "core/EventReceiver.h"
#include "core/TimeEvent.h"
#include "core/GlobalEvent.h"
#include <ibrdtn/data/MetaBundle.h>

#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/RWMutex.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/thread/Queue.h>

#include <string>
#include <list>
#include <set>

namespace dtn
{
	namespace storage
	{
		class SQLiteBundleStorage: public BundleStorage, public dtn::core::EventReceiver<dtn::core::GlobalEvent>, public dtn::core::EventReceiver<dtn::core::TimeEvent>, public dtn::daemon::IndependentComponent, public ibrcommon::BLOB::Provider, public SQLiteDatabase::DatabaseListener
		{
			static const std::string TAG;

		public:
			/**
			 * create a new BLOB object within this storage
			 * @return
			 */
			ibrcommon::BLOB::Reference create();

			/**
			 * Constructor
			 * @param Pfad zum Ordner in denen die Datein gespeichert werden.
			 * @param Dateiname der Datenbank
			 * @param maximale Größe der Datenbank
			 * @param should bundleSets be stored persistently in database? standard: false
			 */
			SQLiteBundleStorage(const ibrcommon::File &path, const dtn::data::Length &maxsize, bool usePersistentBundleSets = false);

			/**
			 * destructor
			 */
			virtual ~SQLiteBundleStorage();

			/**
			 * Stores a bundle in the storage.
			 * @param bundle The bundle to store.
			 */
			void store(const dtn::data::Bundle &bundle);

			/**
			 * This method returns true if the requested bundle is
			 * stored in the storage.
			 */
			bool contains(const dtn::data::BundleID &id);

			/**
			 * Get meta data about a specific bundle ID
			 */
			virtual dtn::data::MetaBundle info(const dtn::data::BundleID &id);

			/**
			 * This method returns a specific bundle which is identified by
			 * its id.
			 * @param id The ID of the bundle to return.
			 * @return A bundle object.
			 */
			dtn::data::Bundle get(const dtn::data::BundleID &id);

			/**
			 * @see BundleSeeker::get(BundleSelector &cb, BundleResult &result)
			 */
			virtual void get(const BundleSelector &cb, BundleResult &result) noexcept (false);

			/**
			 * @see BundleSeeker::getDistinctDestinations()
			 */
			virtual const eid_set getDistinctDestinations();

			/**
			 * This method deletes a specific bundle in the storage.
			 * No reports will be generated here.
			 * @param id The ID of the bundle to remove.
			 */
			void remove(const dtn::data::BundleID &id);

			/**
			 * Clears all bundles and fragments in the storage. Routinginformation won't be deleted.
			 */
			void clear();

			/**
			 * @return True, if no bundles in the storage.
			 */
			bool empty();

			/**
			 * @return the count of bundles in the storage
			 */
			dtn::data::Size count();

			/**
			 * @sa BundleStorage::releaseCustody();
			 */
			void releaseCustody(const dtn::data::EID &custodian, const dtn::data::BundleID &id);

			/**
			 * This method is used to receive events.
			 * @param evt
			 */
			void raiseEvent(const dtn::core::TimeEvent &evt) noexcept;
			void raiseEvent(const dtn::core::GlobalEvent &evt) noexcept;

			/**
			 * callbacks for the sqlite database
			 */
			void eventBundleExpired(const dtn::data::BundleID &id, const dtn::data::Length size) noexcept;
			void iterateDatabase(const dtn::data::MetaBundle &bundle, const dtn::data::Length size);


			/*** BEGIN: methods for unit-testing ***/

			/**
			 * Wait until all the data has been stored to the disk
			 */
			virtual void wait();

			/**
			 * Set the storage to faulty. If set to true, each try to store
			 * or retrieve a bundle will fail.
			 */
			virtual void setFaulty(bool mode);

			/*** END: methods for unit-testing ***/


		protected:
			virtual void componentRun() noexcept;
			virtual void componentUp() noexcept;
			virtual void componentDown() noexcept;
			void __cancellation() noexcept;

		private:
//			enum Position
//			{
//				FIRST_FRAGMENT 	= 0,
//				LAST_FRAGMENT 	= 1,
//				BOTH_FRAGMENTS 	= 2
//			};

			class Task
			{
			public:
				virtual ~Task() {};
				virtual void run(SQLiteBundleStorage &storage) = 0;
			};

			class BlockingTask : public Task
			{
			public:
				BlockingTask() : _done(false), _abort(false) {};
				virtual ~BlockingTask() {};
				virtual void run(SQLiteBundleStorage &storage) = 0;

				/**
				 * wait until this job is done
				 */
				void wait()
				{
					ibrcommon::MutexLock l(_cond);
					while (!_done && !_abort) _cond.wait();

					if (_abort) throw ibrcommon::Exception("Task aborted");
				}

				void abort()
				{
					ibrcommon::MutexLock l(_cond);
					_abort = true;
					_cond.signal(true);
				}

				void done()
				{
					ibrcommon::MutexLock l(_cond);
					_done = true;
					_cond.signal(true);
				}

			private:
				ibrcommon::Conditional _cond;
				bool _done;
				bool _abort;
			};

			class TaskIdle : public Task
			{
			public:
				TaskIdle() { };

				virtual ~TaskIdle() {};
				virtual void run(SQLiteBundleStorage &storage);

				static ibrcommon::Mutex _mutex;
				static bool _idle;
			};

			class TaskExpire : public Task
			{
			public:
				TaskExpire(const dtn::data::Timestamp &timestamp)
				: _timestamp(timestamp) { };

				virtual ~TaskExpire() {};
				virtual void run(SQLiteBundleStorage &storage);

			private:
				const dtn::data::Timestamp _timestamp;
			};

			/**
			 * A SQLiteBLOB is container for large amount of data. Stored in the database
			 * working directory.
			 */
			class SQLiteBLOB : public ibrcommon::BLOB
			{
				friend class SQLiteBundleStorage;
			public:
				virtual ~SQLiteBLOB();

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
				SQLiteBLOB(const ibrcommon::File &path);
				std::fstream _filestream;
				ibrcommon::TemporaryFile _file;
			};


//			/**
//			 *  This Funktion gets e list and a bundle. Every block of the bundle except the PrimaryBlock is saved in a File.
//			 *  The filenames of the blocks are stored in the List. The order of the filenames matches the order of the blocks.
//			 *  @param An empty Stringlist
//			 *  @param A Bundle which should be prepared to be Stored.
//			 *  @return A number bigges than zero is returned indicating an error. Zero is returned if no error occurred.
//			 */
//			int prepareBundle(list<std::string> &filenames, dtn::data::Bundle &bundle);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			SQLiteDatabase _database;

			ibrcommon::File _blobPath;
			ibrcommon::File _blockPath;

			// contains all jobs to do
			ibrcommon::Queue<Task*> _tasks;

			ibrcommon::RWMutex _global_lock;
		};
	}
}

#endif /* SQLITEBUNDLESTORAGE_H_ */
