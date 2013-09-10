/*
 * MemoryBundleSet.cpp
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
 *  Created on: 18.12.2012
 *  renamed from BundleSet.cpp 04.04.2013
 */

#include "ibrdtn/data/MemoryBundleSet.h"

namespace dtn
{
	namespace data
	{

		MemoryBundleSet::MemoryBundleSet(BundleSet::Listener *listener, Length bf_size)
		 : _bf(bf_size * 8), _listener(listener), _consistent(true)
		{
		}

		MemoryBundleSet::~MemoryBundleSet()
		{
		}

		void MemoryBundleSet::add(const dtn::data::MetaBundle &bundle) throw ()
		{
			// insert bundle id to the private list
			pair<std::set<dtn::data::MetaBundle>::iterator,bool> ret = _bundles.insert(bundle);

			BundleSetImpl::ExpiringBundle exb(*ret.first);
			_expire.insert(exb);

			// add bundle to the bloomfilter
			_bf.insert(bundle.toString());
		}

		void MemoryBundleSet::clear() throw ()
		{
			_consistent = true;
			_bundles.clear();
			_expire.clear();
			_bf.clear();
		}

		bool MemoryBundleSet::has(const dtn::data::BundleID &bundle) const throw ()
		{
			// check bloom-filter first
			if (_bf.contains(bundle.toString())) {
				// Return true if the bloom-filter is not consistent with
				// the bundles set. This happen if the MemoryBundleSet gets deserialized.
				if (!_consistent) return true;

				std::set<dtn::data::MetaBundle>::iterator iter = _bundles.find(dtn::data::MetaBundle::mockUp(bundle));
				return (iter != _bundles.end());
			}

			return false;
		}

		Size MemoryBundleSet::size() const throw ()
		{
			return _bundles.size();
		}

		void MemoryBundleSet::expire(const Timestamp timestamp) throw ()
		{
			bool commit = false;

			// we can not expire bundles if we have no idea of time
			if (timestamp == 0) return;

			std::set<BundleSetImpl::ExpiringBundle>::iterator iter = _expire.begin();

			while (iter != _expire.end())
			{
				const BundleSetImpl::ExpiringBundle &b = (*iter);

				if ( b.bundle.expiretime >= timestamp ) break;

				// raise expired event
				if (_listener != NULL)
					_listener->eventBundleExpired( b.bundle );

				// remove this item in public list
				_bundles.erase( b.bundle );

				// remove this item in private list
				_expire.erase( iter++ );

				// set commit to true (triggers bloom-filter rebuild)
				commit = true;
			}

			if (commit)
			{
				// rebuild the bloom-filter
				_bf.clear();
				for (std::set<dtn::data::MetaBundle>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
				{
					_bf.insert( (*iter).toString() );
				}
			}
		}

		const ibrcommon::BloomFilter& MemoryBundleSet::getBloomFilter() const throw ()
		{
			return _bf;
		}

		std::set<dtn::data::MetaBundle> MemoryBundleSet::getNotIn(ibrcommon::BloomFilter &filter) const throw ()
		{
			std::set<dtn::data::MetaBundle> ret;

//			// if the lists are equal return an empty list
//			if (filter == _bf) return ret;

			// iterate through all items to find the differences
			for (std::set<dtn::data::MetaBundle>::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
			{
				if (!filter.contains( (*iter).toString() ) )
				{
					ret.insert( (*iter) );
				}
			}

			return ret;
		}

		Length MemoryBundleSet::getLength() const throw ()
		{
			return dtn::data::Number(_bf.size()).getLength() + _bf.size();
		}

		std::ostream& MemoryBundleSet::serialize(std::ostream &stream) const
		{
			dtn::data::Number size(_bf.size());
			stream << size;

			const char *data = reinterpret_cast<const char*>(_bf.table());
			stream.write(data, _bf.size());

			return stream;
		}

		std::istream& MemoryBundleSet::deserialize(std::istream &stream)
		{
			dtn::data::Number count;
			stream >> count;

			std::vector<char> buffer(count.get<size_t>());

			stream.read(&buffer[0], buffer.size());

			MemoryBundleSet::clear();
			_bf.load((unsigned char*)&buffer[0], buffer.size());

			// set the set to in-consistent mode
			_consistent = false;

			return stream;
		}

		std::string MemoryBundleSet::getType()
		{
			return "MemoryBundleSet";
		}

		bool MemoryBundleSet::isPersistent()
		{
			return false;
		}

		std::string MemoryBundleSet::getName()
		{
			return "";
		}
	} /* namespace data */
} /* namespace dtn */
