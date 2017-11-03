/*
 * NeighborDatabase.cpp
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

#include "routing/NeighborDatabase.h"
#include "core/BundleCore.h"
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>
#include <limits>

namespace dtn
{
	namespace routing
	{
		NeighborDatabase::NeighborEntry::NeighborEntry()
		 : eid(), _filter(), _filter_expire(0), _filter_state(FILTER_EXPIRED, FILTER_FINAL)
		{}

		NeighborDatabase::NeighborEntry::NeighborEntry(const dtn::data::EID &e)
		 : eid(e), _filter(), _filter_expire(0), _filter_state(FILTER_EXPIRED, FILTER_FINAL)
		{ }

		NeighborDatabase::NeighborEntry::~NeighborEntry()
		{
		}

		void NeighborDatabase::NeighborEntry::update(const ibrcommon::BloomFilter &bf, const dtn::data::Number &lifetime)
		{
			ibrcommon::ThreadsafeState<FILTER_REQUEST_STATE>::Locked l = _filter_state.lock();
			_filter = bf;

			if (lifetime == 0)
			{
				_filter_expire = std::numeric_limits<dtn::data::Size>::max();
			}
			else
			{
				_filter_expire = dtn::utils::Clock::getExpireTime(lifetime);
			}

			l = FILTER_AVAILABLE;
		}

		void NeighborDatabase::NeighborEntry::reset()
		{
			ibrcommon::ThreadsafeState<FILTER_REQUEST_STATE>::Locked l = _filter_state.lock();

			l = FILTER_EXPIRED;

			// do not expire again in the next 60 seconds
			_filter_expire = dtn::utils::Clock::getTime() + 60;
		}

		void NeighborDatabase::NeighborEntry::add(const dtn::data::MetaBundle &bundle)
		{
			_summary.add(bundle);
		}

		bool NeighborDatabase::NeighborEntry::has(const dtn::data::BundleID &id, const bool require_bloomfilter) const
		{
			if (_filter_state == FILTER_AVAILABLE)
			{
				if (id.isIn(_filter))
					return true;
			}
			else if (require_bloomfilter)
			{
				throw BloomfilterNotAvailableException(eid);
			}

			return _summary.has(id);
		}

		bool NeighborDatabase::NeighborEntry::isFilterValid() const
		{
			return (_filter_state == FILTER_AVAILABLE);
		}

		bool NeighborDatabase::NeighborEntry::isExpired(const dtn::data::Timestamp &timestamp) const
		{
			// expired after 15 minutes
			return (_last_update + 900) < timestamp;
		}

		const dtn::data::Timestamp& NeighborDatabase::NeighborEntry::getLastUpdate() const
		{
			return _last_update;
		}

		void NeighborDatabase::NeighborEntry::touch()
		{
			_last_update = dtn::utils::Clock::getTime();
		}

		void NeighborDatabase::NeighborEntry::expire(const dtn::data::Timestamp &timestamp)
		{
			{
				ibrcommon::ThreadsafeState<FILTER_REQUEST_STATE>::Locked l = _filter_state.lock();

				if ((_filter_expire > 0) && (_filter_expire < timestamp))
				{
					IBRCOMMON_LOGGER_DEBUG_TAG("NeighborDatabase", 15) << "summary vector of " << eid.getString() << " is expired" << IBRCOMMON_LOGGER_ENDL;

					// set the filter state to expired once
					l = FILTER_EXPIRED;

					// do not expire again in the next 60 seconds
					_filter_expire = timestamp + 60;
				}
			}

			_summary.expire(timestamp);
		}

		void NeighborDatabase::expire(const dtn::data::Timestamp &timestamp)
		{
			for (neighbor_map::iterator iter = _entries.begin(); iter != _entries.end(); )
			{
				NeighborEntry &entry = (*iter->second);

				if (entry.isExpired(timestamp)) {
					delete (*iter).second;
					_entries.erase(iter++);
				} else {
					entry.expire(timestamp);
					++iter;
				}
			}
		}

		void NeighborDatabase::NeighborEntry::acquireTransfer(const dtn::data::BundleID &id) throw (NoMoreTransfersAvailable, AlreadyInTransitException)
		{
			// check if enough resources available to transfer the bundle
			if (_transit_bundles.size() >= dtn::core::BundleCore::max_bundles_in_transit) throw NoMoreTransfersAvailable(eid);

			// check if the bundle is already in transit
			if (_transit_bundles.find(id) != _transit_bundles.end()) throw AlreadyInTransitException();

			// insert the bundle into the transit list
			_transit_bundles.insert(id);

			IBRCOMMON_LOGGER_DEBUG_TAG("NeighborDatabase", 20) << "acquire transfer of " << id.toString() << " to " << eid.getString() << " (" << _transit_bundles.size() << " bundles in transit)" << IBRCOMMON_LOGGER_ENDL;
		}

		dtn::data::Size NeighborDatabase::NeighborEntry::getFreeTransferSlots() const
		{
			const dtn::data::Size transit_bundles = _transit_bundles.size();

			if (dtn::core::BundleCore::max_bundles_in_transit <= transit_bundles) return 0;
			return dtn::core::BundleCore::max_bundles_in_transit - transit_bundles;
		}

		bool NeighborDatabase::NeighborEntry::isTransferThresholdReached() const
		{
			return _transit_bundles.size() <= (dtn::core::BundleCore::max_bundles_in_transit / 2);
		}

		void NeighborDatabase::NeighborEntry::releaseTransfer(const dtn::data::BundleID &id)
		{
			_transit_bundles.erase(id);

			IBRCOMMON_LOGGER_DEBUG_TAG("NeighborDatabase", 20) << "release transfer of " << id.toString() << " to " << eid.getString() << " (" << _transit_bundles.size() << " bundles in transit)" << IBRCOMMON_LOGGER_ENDL;
		}

		void NeighborDatabase::NeighborEntry::putDataset(NeighborDataset &dset)
		{
			std::pair<data_set::iterator, bool> ret = _datasets.insert( dset );

			if (!ret.second) {
				_datasets.erase(ret.first);
				_datasets.insert( dset );
			}
		}

		NeighborDatabase::NeighborDatabase()
		{
		}

		NeighborDatabase::~NeighborDatabase()
		{
			ibrcommon::MutexLock l(*this);
			std::set<dtn::data::EID> ret;

			for (neighbor_map::const_iterator iter = _entries.begin(); iter != _entries.end(); ++iter)
			{
				delete (*iter).second;
			}
		}

		NeighborDatabase::NeighborEntry& NeighborDatabase::create(const dtn::data::EID &eid) throw ()
		{
			neighbor_map::iterator iter = _entries.find(eid);
			if (iter == _entries.end())
			{
				NeighborEntry *entry = new NeighborEntry(eid);
				std::pair<neighbor_map::iterator,bool> itm = _entries.insert( std::pair<dtn::data::EID, NeighborDatabase::NeighborEntry*>(eid, entry) );
				iter = itm.first;
			}

			// set last update timestamp
			(*(*iter).second).touch();

			return *(*iter).second;
		}

		NeighborDatabase::NeighborEntry& NeighborDatabase::get(const dtn::data::EID &eid, bool noCached) throw (EntryNotFoundException)
		{
			if (noCached && !dtn::core::BundleCore::getInstance().getConnectionManager().isNeighbor(eid))
				throw NeighborDatabase::EntryNotFoundException();

			neighbor_map::iterator iter = _entries.find(eid);
			if (iter == _entries.end())
			{
				throw EntryNotFoundException();
			}

			// set last update timestamp
			(*(*iter).second).touch();

			return *(*iter).second;
		}

		void NeighborDatabase::remove(const dtn::data::EID &eid)
		{
			neighbor_map::iterator iter = _entries.find(eid);
			if (iter == _entries.end())
			{
				delete (*iter).second;
				_entries.erase(iter);
			}
		}
	}
}
