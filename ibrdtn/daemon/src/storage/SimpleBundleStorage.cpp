/*
 * SimpleBundleStorage.cpp
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

#include "storage/SimpleBundleStorage.h"
#include "core/EventDispatcher.h"
#include "core/BundleExpiredEvent.h"
#include "core/BundleEvent.h"

#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/thread/RWLock.h>
#include <ibrcommon/Logger.h>

#include <memory>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <cerrno>

namespace dtn
{
	namespace storage
	{
		const std::string SimpleBundleStorage::TAG = "SimpleBundleStorage";

		SimpleBundleStorage::SimpleBundleStorage(const ibrcommon::File &workdir, const dtn::data::Length maxsize, const unsigned int buffer_limit)
		 : BundleStorage(maxsize), _datastore(*this, workdir, buffer_limit), _metastore(this)
		{
		}

		SimpleBundleStorage::~SimpleBundleStorage()
		{
		}

		void SimpleBundleStorage::eventDataStorageStored(const dtn::storage::DataStorage::Hash &hash)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(SimpleBundleStorage::TAG, 30) << "element successfully stored: " << hash.value << IBRCOMMON_LOGGER_ENDL;

			ibrcommon::RWLock l(_pending_lock);
			_pending_bundles.erase(hash);
		}

		void SimpleBundleStorage::eventDataStorageStoreFailed(const dtn::storage::DataStorage::Hash &hash, const ibrcommon::Exception &ex)
		{
			IBRCOMMON_LOGGER_TAG(SimpleBundleStorage::TAG, error) << "store of element " << hash.value << " failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

			dtn::data::MetaBundle meta;

			{
				ibrcommon::MutexLock l(_pending_lock);

				// get the reference to the bundle
				const dtn::data::Bundle &b = _pending_bundles[hash];
				meta = dtn::data::MetaBundle::create(b);
			}

			{
				ibrcommon::RWLock l(_pending_lock);

				// delete the pending bundle
				_pending_bundles.erase(hash);
			}

			ibrcommon::RWLock l(_meta_lock);

			// remove bundle and decrement the storage size
			freeSpace( _metastore.remove(meta) );
		}

		void SimpleBundleStorage::eventDataStorageRemoved(const dtn::storage::DataStorage::Hash &hash)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(SimpleBundleStorage::TAG, 30) << "element successfully removed: " << hash.value << IBRCOMMON_LOGGER_ENDL;

			ibrcommon::RWLock l(_meta_lock);

			for (MetaStorage::const_iterator it = _metastore.begin(); it != _metastore.end(); ++it)
			{
				const dtn::data::MetaBundle &meta = (*it);
				DataStorage::Hash it_hash(BundleContainer::createId(meta));

				if (it_hash == hash)
				{
					// remove bundle and decrement the storage size
					freeSpace( _metastore.remove(meta) );

					return;
				}
			}
		}

		void SimpleBundleStorage::eventDataStorageRemoveFailed(const dtn::storage::DataStorage::Hash &hash, const ibrcommon::Exception &ex)
		{
			IBRCOMMON_LOGGER_TAG(SimpleBundleStorage::TAG, error) << "remove of element " << hash.value << " failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

			// forward this to eventDataStorageRemoved
			eventDataStorageRemoved(hash);
		}

		void SimpleBundleStorage::iterateDataStorage(const dtn::storage::DataStorage::Hash &hash, dtn::storage::DataStorage::istream &stream)
		{
			try {
				dtn::data::Bundle bundle;
				dtn::data::DefaultDeserializer ds(*stream);

				// load a bundle into the storage
				ds >> bundle;
				
				// extract meta data
				const dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(bundle);

				// check if the hash is different
				DataStorage::Hash hash2(BundleContainer::createId(meta));
				if (hash != hash2)
				{
					// if hash does not match, remove old file
					_datastore.remove(hash);

					// and store bundle again
					store(bundle);

					return;
				}

				// allocate space for the bundle
				const dtn::data::Length bundle_size = static_cast<dtn::data::Length>( (*stream).tellg() );
				allocSpace(bundle_size);

				// lock the bundle lists
				ibrcommon::RWLock l(_meta_lock);

				// add the bundle to the stored bundles
				_metastore.store(meta, bundle_size);

				// raise bundle added event
				eventBundleAdded(meta);

				IBRCOMMON_LOGGER_DEBUG_TAG(SimpleBundleStorage::TAG, 10) << "bundle restored " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
			} catch (const std::exception&) {
				// report this error to the console
				IBRCOMMON_LOGGER_TAG(SimpleBundleStorage::TAG, error) << "Unable to restore bundle from file " << hash.value << IBRCOMMON_LOGGER_ENDL;

				// error while reading file
				_datastore.remove(hash);
			}
		}

		void SimpleBundleStorage::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013

			// load persistent bundles
			_datastore.iterateAll();

			// some output
			{
				ibrcommon::MutexLock l(_meta_lock);
				IBRCOMMON_LOGGER_TAG(SimpleBundleStorage::TAG, info) << _metastore.size() << " Bundles restored." << IBRCOMMON_LOGGER_ENDL;
			}

			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);

			try {
				_datastore.start();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG("SimpleBundleStorage", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SimpleBundleStorage::componentDown() noexcept
		{
			// routine checked for noexcept on 15.02.2013

			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
			try {
				_datastore.wait();
				_datastore.stop();
				_datastore.join();

				// reset datastore
				_datastore.reset();

				// clear all data structures
				ibrcommon::RWLock l(_meta_lock);
				_metastore.clear();
				clearSpace();
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG("SimpleBundleStorage", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SimpleBundleStorage::raiseEvent(const dtn::core::TimeEvent &time) noexcept
		{
			if (time.getAction() == dtn::core::TIME_SECOND_TICK)
			{
				ibrcommon::RWLock l(_meta_lock);
				_metastore.expire(time.getTimestamp());
			}
		}

		const std::string SimpleBundleStorage::getName() const
		{
			return "SimpleBundleStorage";
		}

		bool SimpleBundleStorage::empty()
		{
			ibrcommon::MutexLock l(_meta_lock);
			return _metastore.empty();
		}

		void SimpleBundleStorage::releaseCustody(const dtn::data::EID&, const dtn::data::BundleID&)
		{
			// custody is successful transferred to another node.
			// it is safe to delete this bundle now. (depending on the routing algorithm.)
		}

		dtn::data::Size SimpleBundleStorage::count()
		{
			ibrcommon::MutexLock l(_meta_lock);
			return _metastore.size();
		}

		void SimpleBundleStorage::wait()
		{
			_datastore.wait();
		}

		void SimpleBundleStorage::setFaulty(bool mode)
		{
			_faulty = mode;
			_datastore.setFaulty(mode);
		}

		void SimpleBundleStorage::get(const BundleSelector &cb, BundleResult &result) noexcept (false)
		{
			size_t items_added = 0;

			// we have to iterate through all bundles
			ibrcommon::MutexLock l(_meta_lock);

			for (MetaStorage::const_iterator iter = _metastore.begin(); (iter != _metastore.end()) && ((cb.limit() == 0) || (items_added < cb.limit())); ++iter)
			{
				const dtn::data::MetaBundle &meta = (*iter);

				// skip expired bundles
				if ( dtn::utils::Clock::isExpired( meta ) ) continue;

				if ( cb.addIfSelected(result, meta) ) items_added++;
			}

			if (items_added == 0) throw NoBundleFoundException();
		}

		dtn::data::Bundle SimpleBundleStorage::get(const dtn::data::BundleID &id)
		{
			try {
				ibrcommon::MutexLock l(_meta_lock);

				// faulty mechanism for unit-testing
				if (_faulty) {
					throw dtn::SerializationFailedException("bundle get failed due to faulty setting");
				}

				// search for the bundle in the meta storage
				const dtn::data::MetaBundle &meta = _metastore.find(dtn::data::MetaBundle::create(id));

				// create a hash for the data storage
				DataStorage::Hash hash(BundleContainer::createId(meta));

				// check pending bundles
				{
					ibrcommon::MutexLock l(_pending_lock);

					pending_map::iterator it = _pending_bundles.find(hash);

					if (_pending_bundles.end() != it)
					{
						return it->second;
					}
				}

				try {
					DataStorage::istream stream = _datastore.retrieve(hash);

					// load the bundle from the storage
					dtn::data::Bundle bundle;

					// load the bundle from file
					try {
						dtn::data::DefaultDeserializer(*stream) >> bundle;
					} catch (const std::exception &ex) {
						throw dtn::SerializationFailedException(ex.what());
					}

					try {
						dtn::data::AgeBlock &agebl = bundle.find<dtn::data::AgeBlock>();

						// modify the AgeBlock with the age of the file
						time_t age = stream.lastaccess() - stream.lastmodify();

						agebl.addSeconds(age);
					} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };

					return bundle;
				} catch (const DataStorage::DataNotAvailableException &ex) {
					throw dtn::SerializationFailedException(ex.what());
				}
			} catch (const dtn::SerializationFailedException &ex) {
				// bundle loading failed
				IBRCOMMON_LOGGER_TAG(SimpleBundleStorage::TAG, error) << "failed to load bundle: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// the bundle is broken, delete it
				remove(id);

				throw BundleStorage::BundleLoadException(ex.what());
			}

			throw NoBundleFoundException();
		}

		const SimpleBundleStorage::eid_set SimpleBundleStorage::getDistinctDestinations()
		{
			ibrcommon::MutexLock l(_meta_lock);
			return _metastore.getDistinctDestinations();
		}

		void SimpleBundleStorage::store(const dtn::data::Bundle &bundle)
		{
			// get the bundle size
			dtn::data::DefaultSerializer s(std::cout);
			const dtn::data::Length bundle_size = s.getLength(bundle);
			
			// allocate space for the bundle
			allocSpace(bundle_size);

			// accept custody if requested
			try {
				// create meta data object
				const dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(bundle);

				// accept custody
				const dtn::data::EID custodian = BundleStorage::acceptCustody(meta);

				// container for the custody accepted bundle
				dtn::data::Bundle ca_bundle = bundle;

				// set the new custodian
				ca_bundle.custodian = custodian;

				// store the bundle with the custodian
				__store(ca_bundle, bundle_size);
			} catch (const ibrcommon::Exception&) {
				// no custody has been requested - go on with standard store procedure
				__store(bundle, bundle_size);
			}
		}

		bool SimpleBundleStorage::contains(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_meta_lock);

			// search for the bundle in the meta storage
			return _metastore.contains(id);
		}

		dtn::data::MetaBundle SimpleBundleStorage::info(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_meta_lock);

			// search for the bundle in the meta storage
			return _metastore.find(dtn::data::MetaBundle::create(id));
		}

		void SimpleBundleStorage::remove(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_meta_lock);
			const dtn::data::MetaBundle &meta = _metastore.find(dtn::data::MetaBundle::create(id));

			// first check if the bundles is already marked as removed
			if (!_metastore.isRemoved(meta))
			{
				// remove if from the meta storage
				_metastore.markRemoved(meta);

				// create the hash for data storage removal
				DataStorage::Hash hash(BundleContainer::createId(meta));

				// create a background task for removing the bundle
				_datastore.remove(hash);

				// raise bundle removed event
				eventBundleRemoved(meta);
			}
		}

		void SimpleBundleStorage::__store(const dtn::data::Bundle &bundle, const dtn::data::Length &bundle_size)
		{
			// create meta bundle object
			const dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(bundle);

			// create a new container and hash
			std::auto_ptr<BundleContainer> bc(new BundleContainer(bundle));
			DataStorage::Hash hash(*bc);

			// enter critical section - lock pending bundles
			{
				ibrcommon::RWLock l(_pending_lock);

				// add bundle to the pending bundles
				_pending_bundles[hash] = bundle;
			}

			// enter critical section - lock all data structures
			{
				ibrcommon::RWLock l(_meta_lock);

				// add the new bundles to the meta storage
				_metastore.store(meta, bundle_size);
			}

			// put the bundle into the data store
			_datastore.store(hash, bc.get());
			bc.release();

			// raise bundle added event
			eventBundleAdded(meta);
		}

		void SimpleBundleStorage::clear()
		{
			ibrcommon::RWLock l(_meta_lock);

			// mark all bundles for deletion
			for (MetaStorage::const_iterator iter = _metastore.begin(); iter != _metastore.end(); ++iter)
			{
				// remove item in the bundlelist
				const dtn::data::MetaBundle &meta = (*iter);

				DataStorage::Hash hash(BundleContainer::createId(meta));

				// create a background task for removing the bundle
				_datastore.remove(hash);
			}
		}

		void SimpleBundleStorage::eventBundleExpired(const dtn::data::MetaBundle &b) noexcept
		{
			DataStorage::Hash hash(BundleContainer::createId(b));

			// create a background task for removing the bundle
			_datastore.remove(hash);

			// raise bundle event
			dtn::core::BundleEvent::raise( b, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::LIFETIME_EXPIRED);

			// raise an event
			dtn::core::BundleExpiredEvent::raise( b );

			// raise bundle removed event
			eventBundleRemoved(b);
		}

		SimpleBundleStorage::BundleContainer::BundleContainer(const dtn::data::Bundle &b)
		 : _bundle(b)
		{ }

		SimpleBundleStorage::BundleContainer::~BundleContainer()
		{ }

		std::string SimpleBundleStorage::BundleContainer::getId() const
		{
			return createId(_bundle);
		}

		std::string SimpleBundleStorage::BundleContainer::createId(const dtn::data::BundleID &id)
		{
			std::stringstream ss_hash, ss_raw;
			ss_raw << id;

			int c = 0xff & ss_raw.get();
			while (ss_raw.good())
			{
				ss_hash << std::hex << std::setw( 2 ) << std::setfill( '0' ) << c;
				c = 0xff & ss_raw.get();
			}

			return ss_hash.str();
		}

		std::ostream& SimpleBundleStorage::BundleContainer::serialize(std::ostream &stream)
		{
			// get an serializer for bundles
			dtn::data::DefaultSerializer s(stream);

			// length of the bundle
			dtn::data::Length size = s.getLength(_bundle);

			// serialize the bundle
			s << _bundle; stream.flush();

			// check the streams health
			if (!stream.good())
			{
				std::stringstream ss; ss << "Output stream went bad [" << std::strerror(errno) << "]";
				throw dtn::SerializationFailedException(ss.str());
			}

			// get the write position
			if (static_cast<std::streamoff>(size) > stream.tellp())
			{
				std::stringstream ss; ss << "Not all data were written [" << stream.tellp() << " of " << size << " bytes]";
				throw dtn::SerializationFailedException(ss.str());
			}

			// return the stream, this allows stacking
			return stream;
		}
	}
}
