/*
 * MetaStorage.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
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

#include "storage/MetaStorage.h"

namespace dtn
{
	namespace storage
	{
		MetaStorage::MetaStorage(dtn::data::BundleList::Listener *expire_listener)
		 : _list(expire_listener)
		{
		}

		MetaStorage::~MetaStorage()
		{
		}

		bool MetaStorage::contains(const dtn::data::BundleID &id) const noexcept
		{
			return (_bundle_lengths.find(id) != _bundle_lengths.end());
		}

		void MetaStorage::expire(const dtn::data::Timestamp &timestamp) noexcept
		{
			_list.expire(timestamp);
		}

		const dtn::data::MetaBundle& MetaStorage::find(const ibrcommon::BloomFilter &filter) const noexcept (false)
		{
			for (const_iterator iter = begin(); iter != end(); ++iter)
			{
				const dtn::data::MetaBundle &bundle = (*iter);

				// skip removal-marked bundles
				if (_removal_set.find(bundle) != _removal_set.end()) continue;

				if (bundle.isIn(filter))
				{
					return bundle;
				}
			}

			throw NoBundleFoundException();
		}

		std::set<dtn::data::EID> MetaStorage::getDistinctDestinations() const noexcept
		{
			std::set<dtn::data::EID> ret;

			for (dtn::data::BundleList::const_iterator iter = begin(); iter != end(); ++iter)
			{
				const dtn::data::MetaBundle &bundle = (*iter);

				// skip removal-marked bundles
				if (_removal_set.find(bundle) != _removal_set.end()) continue;

				ret.insert(bundle.destination);
			}

			return ret;
		}

		void MetaStorage::store(const dtn::data::MetaBundle &meta, const dtn::data::Length &space) noexcept
		{
			// increment the storage size
			_bundle_lengths[meta] = space;

			// add it to the bundle list
			_list.add(meta);

			// add bundle to priority list
			_priority_index.insert(meta);
		}

		dtn::data::Length MetaStorage::remove(const dtn::data::MetaBundle &meta) noexcept
		{
			// get length of the stored bundle
			size_map::iterator it = _bundle_lengths.find(meta);

			// nothing to remove
			if (it == _bundle_lengths.end()) return 0;

			// store number of bytes for return
			dtn::data::Length ret = (*it).second;

			// remove length entry
			_bundle_lengths.erase(it);

			// remove the bundle from removal set
			_removal_set.erase(meta);

			// remove the bundle from BundleList
			_list.remove(meta);

			// remove bundle from priority index
			_priority_index.erase(meta);

			// return released number of bytes
			return ret;
		}

		void MetaStorage::markRemoved(const dtn::data::MetaBundle &meta) noexcept
		{
			if (contains(meta)) {
				_removal_set.insert(meta);
			}
		}

		bool MetaStorage::isRemoved(const dtn::data::MetaBundle &meta) const noexcept
		{
			return (_removal_set.find(meta) != _removal_set.end());
		}

		bool MetaStorage::empty() noexcept
		{
			if ( _priority_index.empty() )
			{
				return true;
			}

			return size() == 0;
		}

		dtn::data::Size MetaStorage::size() noexcept
		{
			return _priority_index.size() - _removal_set.size();
		}

		void MetaStorage::clear() noexcept
		{
			_priority_index.clear();
			_list.clear();
			_bundle_lengths.clear();
			_removal_set.clear();
		}
	} /* namespace storage */
} /* namespace dtn */
