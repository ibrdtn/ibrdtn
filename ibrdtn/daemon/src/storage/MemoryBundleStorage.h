/*
 * MemoryBundleStorage.h
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

#ifndef MEMORYBUNDLESTORAGE_H_
#define MEMORYBUNDLESTORAGE_H_

#include "Component.h"
#include "core/BundleCore.h"
#include "core/TimeEvent.h"
#include "storage/BundleStorage.h"
#include "core/Node.h"
#include "core/EventReceiver.h"

#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/BundleList.h>

namespace dtn
{
	namespace storage
	{
		class MemoryBundleStorage : public BundleStorage, public dtn::core::EventReceiver<dtn::core::TimeEvent>, public dtn::daemon::IntegratedComponent, public BundleList::Listener
		{
			static const std::string TAG;

		public:
			MemoryBundleStorage(const dtn::data::Length maxsize = 0);
			virtual ~MemoryBundleStorage();

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

		protected:
			virtual void componentUp() noexcept;
			virtual void componentDown() noexcept;

			virtual void eventBundleExpired(const dtn::data::MetaBundle &b) noexcept;

		private:
			ibrcommon::Mutex _bundleslock;

			typedef std::set<dtn::data::Bundle> bundle_list;
			bundle_list _bundles;
			dtn::data::BundleList _list;

			void __erase(const bundle_list::iterator &iter);

			struct CMP_BUNDLE_PRIORITY
			{
				bool operator() (const dtn::data::MetaBundle& lhs, const dtn::data::MetaBundle& rhs) const
				{
					if (lhs.getPriority() > rhs.getPriority())
						return true;

					if (lhs.getPriority() != rhs.getPriority())
						return false;

					return lhs < rhs;
				}
			};

			typedef std::set<dtn::data::MetaBundle, CMP_BUNDLE_PRIORITY> prio_bundle_set;
			prio_bundle_set _priority_index;

			typedef std::map<dtn::data::BundleID, dtn::data::Length> size_map;
			size_map _bundle_lengths;
		};
	}
}

#endif /* MEMORYBUNDLESTORAGE_H_ */
