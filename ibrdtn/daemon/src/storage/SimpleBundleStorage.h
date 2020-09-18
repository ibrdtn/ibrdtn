/*
 * SimpleBundleStorage.h
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

#ifndef SIMPLEBUNDLESTORAGE_H_
#define SIMPLEBUNDLESTORAGE_H_

#include "Component.h"
#include "core/BundleCore.h"
#include "storage/BundleStorage.h"
#include "core/Node.h"
#include "core/EventReceiver.h"
#include "core/TimeEvent.h"

#include "storage/DataStorage.h"
#include "storage/MetaStorage.h"

#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/thread/AtomicCounter.h>
#include <ibrcommon/thread/RWMutex.h>

#include <ibrcommon/data/File.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/BundleList.h>
#include <ibrcommon/thread/Queue.h>

#include <set>
#include <map>

namespace dtn
{
	namespace storage
	{
		/**
		 * This storage holds all bundles and fragments in the system memory.
		 */
		class SimpleBundleStorage : public DataStorage::Callback, public BundleStorage, public dtn::core::EventReceiver<dtn::core::TimeEvent>, public dtn::daemon::IntegratedComponent, public dtn::data::BundleList::Listener
		{
			static const std::string TAG;

		public:
			/**
			 * Constructor
			 */
			SimpleBundleStorage(const ibrcommon::File &workdir, const dtn::data::Length maxsize = 0, const unsigned int buffer_limit = 0);

			/**
			 * Destructor
			 */
			virtual ~SimpleBundleStorage();

			/**
			 * Stores a bundle in the storage.
			 * @param bundle The bundle to store.
			 */
			virtual void store(const dtn::data::Bundle &bundle);

			/**
			 * This method returns true if the requested bundle is
			 * stored in the storage.
			 */
			virtual bool contains(const dtn::data::BundleID &id);

			/**
			 * Get meta data about a specific bundle ID
			 */
			virtual dtn::data::MetaBundle info(const dtn::data::BundleID &id);

			/**
			 * This method returns a specific bundle which is identified by
			 * its id.
			 * @param id The ID of the bundle to return.
			 * @return A bundle object of the
			 */
			virtual dtn::data::Bundle get(const dtn::data::BundleID &id);

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
			 * @sa BundleStorage::clear()
			 */
			void clear();

			/**
			 * @sa BundleStorage::empty()
			 */
			bool empty();

			/**
			 * @sa BundleStorage::count()
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

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			virtual void eventDataStorageStored(const dtn::storage::DataStorage::Hash &hash);
			virtual void eventDataStorageStoreFailed(const dtn::storage::DataStorage::Hash &hash, const ibrcommon::Exception&);
			virtual void eventDataStorageRemoved(const dtn::storage::DataStorage::Hash &hash);
			virtual void eventDataStorageRemoveFailed(const dtn::storage::DataStorage::Hash &hash, const ibrcommon::Exception&);
			virtual void iterateDataStorage(const dtn::storage::DataStorage::Hash &hash, dtn::storage::DataStorage::istream &stream);

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
			virtual void componentUp() noexcept;
			virtual void componentDown() noexcept;
			virtual void eventBundleExpired(const dtn::data::MetaBundle &b) noexcept;

		private:
			class BundleContainer : public DataStorage::Container
			{
			public:
				BundleContainer(const dtn::data::Bundle &b);
				virtual ~BundleContainer();

				/**
				 * create a unique identifier from the bundle ID
				 */
				static std::string createId(const dtn::data::BundleID &id);

				/**
				 * get the unique identifier for this bundle container
				 */
				std::string getId() const;

				/**
				 * write the container to a stream object
				 */
				std::ostream& serialize(std::ostream &stream);

			private:
				const dtn::data::Bundle _bundle;
			};

			void __remove(const dtn::data::MetaBundle &meta);
			void __store(const dtn::data::Bundle &bundle, const dtn::data::Length &bundle_size);

			typedef std::map<DataStorage::Hash, dtn::data::Bundle> pending_map;
			ibrcommon::RWMutex _pending_lock;
			pending_map _pending_bundles;

			// This object manages data stored on disk
			DataStorage _datastore;

			// stores all the meta data in memory
			ibrcommon::RWMutex _meta_lock;
			MetaStorage _metastore;
		};
	}
}

#endif /*SIMPLEBUNDLESTORAGE_H_*/
