/*
 * MemoryBundleStorage.cpp
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

#include "storage/MemoryBundleStorage.h"
#include "core/EventDispatcher.h"
#include "core/BundleExpiredEvent.h"
#include "core/BundleEvent.h"

#include <ibrdtn/utils/Clock.h>

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

namespace dtn
{
	namespace storage
	{
		const std::string MemoryBundleStorage::TAG = "MemoryBundleStorage";

		MemoryBundleStorage::MemoryBundleStorage(const dtn::data::Length maxsize)
		 : BundleStorage(maxsize), _list(this)
		{
		}

		MemoryBundleStorage::~MemoryBundleStorage()
		{
		}

		void MemoryBundleStorage::componentUp() throw ()
		{
			// routine checked for throw() on 15.02.2013
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
		}

		void MemoryBundleStorage::componentDown() throw ()
		{
			// routine checked for throw() on 15.02.2013
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
		}

		void MemoryBundleStorage::raiseEvent(const dtn::core::TimeEvent &time) throw ()
		{
			if (time.getAction() == dtn::core::TIME_SECOND_TICK)
			{
				// do expiration of bundles
				ibrcommon::MutexLock l(_bundleslock);
				_list.expire(time.getTimestamp());
			}
		}

		const std::string MemoryBundleStorage::getName() const
		{
			return MemoryBundleStorage::TAG;
		}

		bool MemoryBundleStorage::empty()
		{
			ibrcommon::MutexLock l(_bundleslock);
			return _bundles.empty();
		}

		void MemoryBundleStorage::releaseCustody(const dtn::data::EID&, const dtn::data::BundleID&)
		{
			// custody is successful transferred to another node.
			// it is safe to delete this bundle now. (depending on the routing algorithm.)
		}

		dtn::data::Size MemoryBundleStorage::count()
		{
			ibrcommon::MutexLock l(_bundleslock);
			return _bundles.size();
		}

		void MemoryBundleStorage::get(const BundleSelector &cb, BundleResult &result) throw (NoBundleFoundException, BundleSelectorException)
		{
			size_t items_added = 0;

			// we have to iterate through all bundles
			ibrcommon::MutexLock l(_bundleslock);

			for (prio_bundle_set::const_iterator iter = _priority_index.begin(); (iter != _priority_index.end()) && ((cb.limit() == 0) || (items_added < cb.limit())); ++iter)
			{
				const dtn::data::MetaBundle &bundle = (*iter);

				// skip expired bundles
				if ( dtn::utils::Clock::isExpired( bundle ) ) continue;

				if ( cb.addIfSelected(result, bundle) ) items_added++;
			}

			if (items_added == 0) throw NoBundleFoundException();
		}

		dtn::data::Bundle MemoryBundleStorage::get(const dtn::data::BundleID &id)
		{
			try {
				ibrcommon::MutexLock l(_bundleslock);

				for (bundle_list::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
				{
					const dtn::data::Bundle &bundle = (*iter);

					// the bundles are sorted, therefore it is possible to optimize
					// the search for the bundle by skipping all "lower" bundles
					if (id > bundle) continue;

					if (id == bundle)
					{
						if (_faulty) {
							throw dtn::SerializationFailedException("bundle get failed due to faulty setting");
						}

						return bundle;
					}

					// stop here, following bundles are "greater"
					break;
				}
			} catch (const dtn::SerializationFailedException &ex) {
				// bundle loading failed
				IBRCOMMON_LOGGER_TAG(MemoryBundleStorage::TAG, error) << "Error while loading bundle data: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// the bundle is broken, delete it
				remove(id);

				throw BundleStorage::BundleLoadException();
			}

			throw NoBundleFoundException();
		}

		const MemoryBundleStorage::eid_set MemoryBundleStorage::getDistinctDestinations()
		{
			std::set<dtn::data::EID> ret;

			ibrcommon::MutexLock l(_bundleslock);

			for (bundle_list::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
			{
				const dtn::data::Bundle &bundle = (*iter);
				ret.insert(bundle.destination);
			}

			return ret;
		}

		void MemoryBundleStorage::store(const dtn::data::Bundle &bundle)
		{
			ibrcommon::MutexLock l(_bundleslock);

			if (_faulty) return;

			// get size of the bundle
			dtn::data::DefaultSerializer s(std::cout);
			dtn::data::Length size = s.getLength(bundle);

			// increment the storage size
			allocSpace(size);

			// insert Container
			std::pair<std::set<dtn::data::Bundle>::iterator,bool> ret = _bundles.insert( bundle );

			if (ret.second)
			{
				const dtn::data::MetaBundle m = dtn::data::MetaBundle::create(bundle);
				_list.add(m);
				_priority_index.insert(m);

				_bundle_lengths[m] = size;

				// raise bundle added event
				eventBundleAdded(m);
			}
			else
			{
				// free the previously allocated space
				freeSpace(size);

				IBRCOMMON_LOGGER_DEBUG_TAG(MemoryBundleStorage::TAG, 5) << "got bundle duplicate " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		bool MemoryBundleStorage::contains(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_bundleslock);
			return (_bundle_lengths.find(id) != _bundle_lengths.end());
		}

		dtn::data::MetaBundle MemoryBundleStorage::info(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_bundleslock);

			for (dtn::data::BundleList::const_iterator iter = _list.begin(); iter != _list.end(); ++iter)
			{
				const dtn::data::MetaBundle &meta = (*iter);
				if (id == meta)
				{
					return meta;
				}
			}

			throw NoBundleFoundException();
		}

		void MemoryBundleStorage::remove(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_bundleslock);

			// search for the bundle in the bundle list
			const bundle_list::const_iterator iter = find(_bundles.begin(), _bundles.end(), id);

			// if no bundle was found throw an exception
			if (iter == _bundles.end()) throw NoBundleFoundException();

			// remove item in the bundlelist
			const dtn::data::MetaBundle m = dtn::data::MetaBundle::create(*iter);
			_list.remove(m);

			// raise bundle removed event
			eventBundleRemoved(m);

			// erase the bundle
			__erase(iter);
		}

		void MemoryBundleStorage::clear()
		{
			ibrcommon::MutexLock l(_bundleslock);

			for (bundle_list::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
			{
				const dtn::data::Bundle &bundle = (*iter);

				// raise bundle removed event
				eventBundleRemoved(bundle);
			}

			_bundles.clear();
			_priority_index.clear();
			_list.clear();
			_bundle_lengths.clear();

			// set the storage size to zero
			clearSpace();
		}

		void MemoryBundleStorage::eventBundleExpired(const dtn::data::MetaBundle &b) throw ()
		{
			// search for the bundle in the bundle list
			const bundle_list::iterator iter = find(_bundles.begin(), _bundles.end(), b);

			// if the bundle was found ...
			if (iter != _bundles.end())
			{
				// raise bundle removed event
				eventBundleRemoved(b);

				// raise bundle event
				dtn::core::BundleEvent::raise( b, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::LIFETIME_EXPIRED);

				// raise an event
				dtn::core::BundleExpiredEvent::raise( b );

				// erase the bundle
				__erase(iter);
			}
		}

		void MemoryBundleStorage::__erase(const bundle_list::iterator &iter)
		{
			const dtn::data::MetaBundle m = dtn::data::MetaBundle::create(*iter);

			// erase the bundle out of the priority index
			_priority_index.erase(m);

			// get the storage size of this bundle
			dtn::data::Length len = _bundle_lengths[m];
			_bundle_lengths.erase(m);

			// decrement the storage size
			freeSpace(len);

			// remove bundle from bundle list
			_bundles.erase(iter);
		}
	}
}
