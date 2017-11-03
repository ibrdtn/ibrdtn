/*
 * SQLiteBundleStorage.cpp
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


#include "storage/SQLiteBundleStorage.h"
#include "core/EventDispatcher.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"

#include <ibrdtn/data/BundleBuilder.h>
#include <ibrdtn/data/PayloadBlock.h>
#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/data/Serializer.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/BundleID.h>

#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/RWLock.h>
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/Logger.h>
#include <memory>
#include <unistd.h>

namespace dtn
{
	namespace storage
	{
		const std::string SQLiteBundleStorage::TAG = "SQLiteBundleStorage";

		ibrcommon::Mutex SQLiteBundleStorage::TaskIdle::_mutex;
		bool SQLiteBundleStorage::TaskIdle::_idle = false;

		SQLiteBundleStorage::SQLiteBLOB::SQLiteBLOB(const ibrcommon::File &path)
		 : _file(path, "blob")
		{
		}

		SQLiteBundleStorage::SQLiteBLOB::~SQLiteBLOB()
		{
			// delete the file if the last reference is destroyed
			_file.remove();
		}

		void SQLiteBundleStorage::SQLiteBLOB::clear()
		{
			// close the file
			_filestream.close();

			// open temporary file
			_filestream.open(_file.getPath().c_str(), std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary );

			if (!_filestream.is_open())
			{
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, error) << "can not open temporary file " << _file.getPath() << IBRCOMMON_LOGGER_ENDL;
				throw ibrcommon::CanNotOpenFileException(_file);
			}
		}

		void SQLiteBundleStorage::SQLiteBLOB::open()
		{
			ibrcommon::BLOB::_filelimit.wait();

			// open temporary file
			_filestream.open(_file.getPath().c_str(), std::ios::in | std::ios::out | std::ios::binary );

			if (!_filestream.is_open())
			{
				// Release semaphore on failed file open
				ibrcommon::BLOB::_filelimit.post();

				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, error) << "can not open temporary file " << _file.getPath() << IBRCOMMON_LOGGER_ENDL;
				throw ibrcommon::CanNotOpenFileException(_file);
			}
		}

		void SQLiteBundleStorage::SQLiteBLOB::close()
		{
			// flush the filestream
			_filestream.flush();

			// close the file
			_filestream.close();

			ibrcommon::BLOB::_filelimit.post();
		}

		std::streamsize SQLiteBundleStorage::SQLiteBLOB::__get_size()
		{
			return _file.size();
		}

		ibrcommon::BLOB::Reference SQLiteBundleStorage::create()
		{
			return ibrcommon::BLOB::Reference(new SQLiteBLOB(_blobPath));
		}

		SQLiteBundleStorage::SQLiteBundleStorage(const ibrcommon::File &path, const dtn::data::Length &maxsize, bool usePersistentBundleSets)
		 : BundleStorage(maxsize), _database(path.get("sqlite.db"), *this)
		{
			//let the factory create SQLiteBundleSets
			if (usePersistentBundleSets)
				dtn::data::BundleSet::setFactory(new dtn::storage::SQLiteBundleSet::Factory(_database));

			// use sqlite storage as BLOB provider, auto delete off
			ibrcommon::BLOB::changeProvider(this, false);

			// set the block path
			_blockPath = path.get("blocks");
			_blobPath = path.get("blob");

			try {
				ibrcommon::RWLock l(_global_lock);

				// delete all old BLOB container
				_blobPath.remove(true);

				// create BLOB folder
				ibrcommon::File::createDirectory( _blobPath );

				// create the bundle folder
				ibrcommon::File::createDirectory( _blockPath );

				// open the database and create all folders and files if needed
				_database.open();
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		SQLiteBundleStorage::~SQLiteBundleStorage()
		{
			// stop factory from creating SQLiteBundleSets
			dtn::data::BundleSet::setFactory(NULL);

			try {
				ibrcommon::RWLock l(_global_lock);

				// close the database
				_database.close();
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SQLiteBundleStorage::componentRun() throw ()
		{
			// loop until aborted
			try {
				while (true)
				{
					Task *t = _tasks.poll();

					try {
						BlockingTask &btask = dynamic_cast<BlockingTask&>(*t);
						try {
							btask.run(*this);
						} catch (const std::exception&) {
							btask.abort();
							continue;
						};
						btask.done();
						continue;
					} catch (const std::bad_cast&) { };

					try {
						std::auto_ptr<Task> killer(t);
						t->run(*this);
					} catch (const std::exception&) { };
				}
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				// we are aborted, abort all blocking tasks
			}
		}

		void SQLiteBundleStorage::componentUp() throw ()
		{
			// routine checked for throw() on 15.02.2013

			//register Events
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::GlobalEvent>::add(this);

			try {
				// iterate through all bundles to generate indexes
				_database.iterateAll();
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SQLiteBundleStorage::componentDown() throw ()
		{
			// routine checked for throw() on 15.02.2013

			//unregister Events
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::GlobalEvent>::remove(this);

			stop();
			join();
		}

		void SQLiteBundleStorage::__cancellation() throw ()
		{
			_tasks.abort();
		}

		const SQLiteBundleStorage::eid_set SQLiteBundleStorage::getDistinctDestinations()
		{
			try {
				ibrcommon::MutexLock l(_global_lock);
				return _database.getDistinctDestinations();
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
			return SQLiteBundleStorage::eid_set();
		}

		void SQLiteBundleStorage::get(const BundleSelector &cb, BundleResult &result) throw (NoBundleFoundException, BundleSelectorException)
		{
			ibrcommon::MutexLock l(_global_lock);
			_database.get(cb, result);
		}

		dtn::data::Bundle SQLiteBundleStorage::get(const dtn::data::BundleID &id)
		{
			SQLiteDatabase::blocklist blocks;
			dtn::data::Bundle bundle;

			try {
				ibrcommon::MutexLock l(_global_lock);

				// query the data base for the bundle
				_database.get(id, bundle, blocks);

				for (SQLiteDatabase::blocklist::const_iterator iter = blocks.begin(); iter != blocks.end(); ++iter)
				{
					const SQLiteDatabase::blocklist_entry &entry = (*iter);
					const int blocktyp = entry.first;
					const ibrcommon::File &file = entry.second;

					IBRCOMMON_LOGGER_DEBUG_TAG(SQLiteBundleStorage::TAG, 50) << "add block: " << file.getPath() << IBRCOMMON_LOGGER_ENDL;

					// load block from file
					std::ifstream is(file.getPath().c_str(), std::ios::binary | std::ios::in);

					if (blocktyp == dtn::data::PayloadBlock::BLOCK_TYPE)
					{
						// create a new BLOB object
						SQLiteBLOB *blob = new SQLiteBLOB(_blobPath);

						// create a reference of the BLOB
						ibrcommon::BLOB::Reference ref(blob);

						try {
							// remove the temporary file
							blob->_file.remove();

							// generate a hard-link, pointing to the BLOB file
							if ( ::link(file.getPath().c_str(), blob->_file.getPath().c_str()) != 0 )
							{
								IBRCOMMON_LOGGER_DEBUG_TAG(SQLiteBundleStorage::TAG, 25) << "hard-link failed (" << errno << ") " << blob->_file.getPath() << " -> " << file.getPath() << IBRCOMMON_LOGGER_ENDL;

								// copy the BLOB into a new file if hard-links are not supported
								std::ofstream fout(blob->_file.getPath().c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

								// open the filestream
								ibrcommon::BLOB::iostream stream = ref.iostream();

								const std::streamsize length = stream.size();
								ibrcommon::BLOB::copy(fout, (*stream), length);
							}
							else
							{
								// update BLOB size
								blob->update();
							}

							// add payload block to the bundle
							bundle.push_back(ref);
						} catch (const ibrcommon::Exception &ex) {
							// remove the temporary file
							blob->_file.remove();

							IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, error) << "unable to load bundle: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							throw dtn::SerializationFailedException(ex.what());
						}
					}
					else
					{
						try {
							// read the block
							dtn::data::Block &block = dtn::data::SeparateDeserializer(is, bundle).readBlock();

							// close the file
							is.close();

							// modify the age block if present
							try {
								dtn::data::AgeBlock &agebl = dynamic_cast<dtn::data::AgeBlock&>(block);

								// modify the AgeBlock with the age of the file
								time_t age = file.lastaccess() - file.lastmodify();

								agebl.addSeconds(age);
							} catch (const std::bad_cast&) { };
						} catch (dtn::data::BundleBuilder::DiscardBlockException &ex) {
							// skip extensions block
						}
					}
				}
			} catch (const SQLiteDatabase::SQLiteQueryException &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// try to purge all referenced data
				remove(id);

				throw dtn::storage::NoBundleFoundException();
			}

			return bundle;
		}

		void SQLiteBundleStorage::store(const dtn::data::Bundle &bundle)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(SQLiteBundleStorage::TAG, 25) << "store bundle " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

			ibrcommon::RWLock l(_global_lock);

			// get size of the bundle
			dtn::data::DefaultSerializer s(std::cout);
			dtn::data::Length size = s.getLength(bundle);

			// increment the storage size
			allocSpace(size);

			// start transaction to store the bundle
			_database.transaction();

			try {
				// store the bundle data in the database
				_database.store(bundle, size);

				// create a bundle id
				const dtn::data::BundleID &id = bundle;
				const dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(bundle);

				// index number for order of the blocks
				int index = 1;

				// number of bytes stored
				dtn::data::Length storedBytes = 0;

				for(dtn::data::Bundle::const_iterator it = bundle.begin() ;it != bundle.end(); ++it)
				{
					const dtn::data::Block &block = (**it);

					if (block.getType() == dtn::data::PayloadBlock::BLOCK_TYPE)
					{
						// create a temporary file
						ibrcommon::TemporaryFile tmpfile(_blockPath, "payload");

						try {
							const dtn::data::PayloadBlock &payload = dynamic_cast<const dtn::data::PayloadBlock&>(block);
							ibrcommon::BLOB::Reference ref = payload.getBLOB();
							ibrcommon::BLOB::iostream stream = ref.iostream();

							try {
								const SQLiteBLOB &blob = dynamic_cast<const SQLiteBLOB&>(*ref);

								// first remove the tmp file
								tmpfile.remove();

								// make a hard-link to the origin blob file
								if ( ::link(blob._file.getPath().c_str(), tmpfile.getPath().c_str()) != 0 )
								{
									IBRCOMMON_LOGGER_DEBUG_TAG(SQLiteBundleStorage::TAG, 25) << "hard-link failed (" << errno << ") " << tmpfile.getPath() << " -> " << blob._file.getPath() << IBRCOMMON_LOGGER_ENDL;

									// copy the BLOB into a new file if hard-links are not supported
									std::ofstream fout(tmpfile.getPath().c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

									const std::streamsize length = stream.size();
									ibrcommon::BLOB::copy(fout, (*stream), length);
								}
							} catch (const std::bad_cast&) {
								// copy the BLOB into a new file this isn't a sqlite block object
								std::ofstream fout(tmpfile.getPath().c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);

								const std::streamsize length = stream.size();
								ibrcommon::BLOB::copy(fout, (*stream), length);
							}
						} catch (const std::bad_cast&) {
							// remove the tmp file
							tmpfile.remove();
							throw ibrcommon::Exception("not a payload block");
						}

						// add determine the amount of stored bytes
						storedBytes += tmpfile.size();

						// store the block into the database
						_database.store(id, index, block, tmpfile);
					}
					else
					{
						ibrcommon::TemporaryFile tmpfile(_blockPath, "block");

						std::ofstream filestream(tmpfile.getPath().c_str(), std::ofstream::out | std::ofstream::trunc | std::ofstream::binary);
						dtn::data::SeparateSerializer serializer(filestream);
						serializer << block;
						filestream.close();

						// add determine the amount of stored bytes
						storedBytes += tmpfile.size();

						// store the block into the database
						_database.store(id, index, block, tmpfile);
					}

					// increment index
					index++;
				}

				_database.commit();

				try {
					// the bundle is stored sucessfully, we could accept custody if it is requested
					const dtn::data::EID custodian = acceptCustody(meta);

					// update the custody address of this bundle
					_database.update(SQLiteDatabase::UPDATE_CUSTODIAN, bundle, custodian);
				} catch (const ibrcommon::Exception&) {
					// this bundle has no request for custody transfers
				}

				IBRCOMMON_LOGGER_DEBUG_TAG(SQLiteBundleStorage::TAG, 10) << "bundle " << bundle.toString() << " stored" << IBRCOMMON_LOGGER_ENDL;

				// raise bundle added event
				eventBundleAdded(meta);
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_database.rollback();

				// free the previously allocated space
				freeSpace(size);
			}
		}

		bool SQLiteBundleStorage::contains(const dtn::data::BundleID &id)
		{
			try {
				ibrcommon::MutexLock l(_global_lock);
				return _database.contains(id);
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				return false;
			}
		}

		dtn::data::MetaBundle SQLiteBundleStorage::info(const dtn::data::BundleID &id)
		{
			try {
				ibrcommon::MutexLock l(_global_lock);
				dtn::data::MetaBundle ret;
				_database.get(id, ret);
				return ret;
			} catch (const SQLiteDatabase::SQLiteQueryException&) {
				throw dtn::storage::NoBundleFoundException();
			}
		}

		void SQLiteBundleStorage::remove(const dtn::data::BundleID &id)
		{
			// remove the bundle in locked state
			try {
				ibrcommon::RWLock l(_global_lock);
				freeSpace( _database.remove(id) );

				// raise bundle removed event
				eventBundleRemoved(id);
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SQLiteBundleStorage::clear()
		{
			ibrcommon::RWLock l(_global_lock);

			try {
				_database.clear();
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

			//Delete Folder SQL_TABLE_BLOCK containing Blocks
			_blockPath.remove(true);
			ibrcommon::File::createDirectory(_blockPath);

			// set the storage size to zero
			clearSpace();
		}

		bool SQLiteBundleStorage::empty()
		{
			try {
				ibrcommon::MutexLock l(_global_lock);
				return _database.empty();
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
			return true;
		}

		dtn::data::Size SQLiteBundleStorage::count()
		{
			try {
				ibrcommon::MutexLock l(_global_lock);
				return _database.count();
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
			return 0;
		}

		void SQLiteBundleStorage::raiseEvent(const dtn::core::TimeEvent &time) throw ()
		{
			if (time.getAction() == dtn::core::TIME_SECOND_TICK)
			{
				_tasks.push(new TaskExpire(time.getTimestamp()));
			}
		}

		void SQLiteBundleStorage::raiseEvent(const dtn::core::GlobalEvent &global) throw ()
		{
			if (global.getAction() == dtn::core::GlobalEvent::GLOBAL_IDLE)
			{
				// switch to idle mode
				ibrcommon::MutexLock l(TaskIdle::_mutex);
				TaskIdle::_idle = true;

				// generate an idle task
				_tasks.push(new TaskIdle());
			}
			else if (global.getAction() == dtn::core::GlobalEvent::GLOBAL_BUSY)
			{
				// switch back to non-idle mode
				ibrcommon::MutexLock l(TaskIdle::_mutex);
				TaskIdle::_idle = false;
			}
		}

		void SQLiteBundleStorage::TaskExpire::run(SQLiteBundleStorage &storage)
		{
			try {
				ibrcommon::RWLock l(storage._global_lock);
				storage._database.expire(_timestamp);
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SQLiteBundleStorage::TaskIdle::run(SQLiteBundleStorage &storage)
		{
			// until IDLE is false
			while (true)
			{
				/*
				 * When an object (table, index, trigger, or view) is dropped from the database, it leaves behind empty space.
				 * This empty space will be reused the next time new information is added to the database. But in the meantime,
				 * the database file might be larger than strictly necessary. Also, frequent inserts, updates, and deletes can
				 * cause the information in the database to become fragmented - scrattered out all across the database file rather
				 * than clustered together in one place.
				 * The VACUUM command cleans the main database. This eliminates free pages, aligns table data to be contiguous,
				 * and otherwise cleans up the database file structure.
				 */
				try {
					ibrcommon::RWLock l(storage._global_lock);
					storage._database.vacuum();
				} catch (const ibrcommon::Exception &ex) {
					IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}

				// here we can do some IDLE stuff...
				ibrcommon::Thread::sleep(1000);

				ibrcommon::MutexLock l(TaskIdle::_mutex);
				if (!TaskIdle::_idle) return;
			}
		}

		const std::string SQLiteBundleStorage::getName() const
		{
			return "SQLiteBundleStorage";
		}

		void SQLiteBundleStorage::releaseCustody(const dtn::data::EID &custodian, const dtn::data::BundleID &id)
		{
			try {
				ibrcommon::MutexLock l(_global_lock);

				// custody is successful transferred to another node.
				// it is safe to delete this bundle now. (depending on the routing algorithm.)
				// update the custodian of this bundle with the new one
				_database.update(SQLiteDatabase::UPDATE_CUSTODIAN, id, custodian);
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(SQLiteBundleStorage::TAG, critical) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SQLiteBundleStorage::iterateDatabase(const dtn::data::MetaBundle &bundle, const dtn::data::Length size)
		{
			// raise bundle added event
			eventBundleAdded(bundle);

			// allocate consumed space of the bundle
			allocSpace(size);
		}

		void SQLiteBundleStorage::eventBundleExpired(const dtn::data::BundleID &id, const dtn::data::Length size) throw ()
		{
			// raise bundle removed event
			eventBundleRemoved(id);

			// release consumed space of this bundle
			freeSpace(size);
		}

		void SQLiteBundleStorage::wait()
		{
			_tasks.wait(ibrcommon::Queue<Task*>::QUEUE_EMPTY);
		}

		void SQLiteBundleStorage::setFaulty(bool mode)
		{
			_faulty = mode;
			_database.setFaulty(mode);
		}
	}
}
