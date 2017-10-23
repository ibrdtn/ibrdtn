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
		ibrcommon::File MemoryBundleSet::__store_path__;
		bool MemoryBundleSet::__store_path_set__ = false;

		MemoryBundleSet::MemoryBundleSet(BundleSet::Listener *listener, Length bf_size)
		 : _name(), _bf_size(bf_size), _bf(bf_size), _listener(listener), _consistent(true)
		{
		}

		MemoryBundleSet::MemoryBundleSet(const std::string &name, BundleSet::Listener *listener, Length bf_size)
		 : _name(name), _bf_size(bf_size), _bf(bf_size), _listener(listener), _consistent(true)
		{
			try {
				restore();
			} catch (const dtn::InvalidDataException &ex) {
				// restore failed to invalid data
			} catch (const std::exception &ex) {
				// restore failed
			}
		}

		MemoryBundleSet::~MemoryBundleSet()
		{
			store();
		}

		refcnt_ptr<BundleSetImpl> MemoryBundleSet::copy() const
		{
			MemoryBundleSet *set = new MemoryBundleSet(_listener, _bf_size);

			// copy Bloom-filter
			set->_bf_size = _bf_size;
			set->_bf = _bf;
			set->_consistent = _consistent;

			// copy bundles
			set->_bundles.insert(_bundles.begin(), _bundles.end());

			// create a new expiration set
			for (bundle_set::const_iterator it = set->_bundles.begin(); it != set->_bundles.end(); ++it)
			{
				BundleSetImpl::ExpiringBundle exb(*it);
				set->_expire.insert(exb);
			}

			return refcnt_ptr<BundleSetImpl>(set);
		}

		void MemoryBundleSet::assign(const refcnt_ptr<BundleSetImpl> &other)
		{
			// clear all bundles first
			clear();

			try {
				// cast the given set to a MemoryBundleSet
				const MemoryBundleSet &set = dynamic_cast<const MemoryBundleSet&>(*other);

				// copy Bloom-filter
				_bf_size = set._bf_size;
				_bf = set._bf;
				_consistent = set._consistent;

				// clear bundles and expiration set
				_bundles.clear();
				_expire.clear();

				// copy bundles
				_bundles.insert(set._bundles.begin(), set._bundles.end());

				// create a new expiration set
				for (bundle_set::const_iterator it = _bundles.begin(); it != _bundles.end(); ++it)
				{
					BundleSetImpl::ExpiringBundle exb(*it);
					_expire.insert(exb);
				}
			} catch (const std::bad_cast&) {
				// incompatible bundle-set implementation - abort here
			}
		}

		void MemoryBundleSet::add(const dtn::data::MetaBundle &bundle) throw ()
		{
			// insert bundle id to the private list
			std::pair<bundle_set::iterator,bool> ret = _bundles.insert(bundle);

			BundleSetImpl::ExpiringBundle exb(*ret.first);
			_expire.insert(exb);

			// increase the size of the Bloom-filter if the allocation is too high
			if (_consistent && _bf.grow(_bundles.size() + 1))
			{
				// re-insert all bundles
				for (bundle_set::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
				{
					(*iter).addTo(_bf);
				}
			}
			else
			{
				// add bundle to the bloomfilter
				bundle.addTo(_bf);
			}
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
			if (bundle.isIn(_bf)) {
				// Return true if the bloom-filter is not consistent with
				// the bundles set. This happen if the MemoryBundleSet gets deserialized.
				if (!_consistent) return true;

				bundle_set::iterator iter = _bundles.find(dtn::data::MetaBundle::create(bundle));
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

			expire_set::iterator iter = _expire.begin();

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

				// get number of elements
				const size_t bnum = size();

				// increase the size of the Bloom-filter if the allocation is too high
				_bf.grow(bnum);

				for (bundle_set::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
				{
					(*iter).addTo(_bf);
				}
			}
		}

		const ibrcommon::BloomFilter& MemoryBundleSet::getBloomFilter() const throw ()
		{
			return _bf;
		}

		MemoryBundleSet::bundle_set MemoryBundleSet::getNotIn(const ibrcommon::BloomFilter &filter) const throw ()
		{
			bundle_set ret;

//			// if the lists are equal return an empty list
//			if (filter == _bf) return ret;

			// iterate through all items to find the differences
			for (bundle_set::const_iterator iter = _bundles.begin(); iter != _bundles.end(); ++iter)
			{
				if (!(*iter).isIn(filter))
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

		void MemoryBundleSet::setPath(const ibrcommon::File &path)
		{
			// copy the file object
			ibrcommon::File p = path;

			// create path
			ibrcommon::File::createDirectory(p);

			if (p.exists() && p.isDirectory()) {
				__store_path__ = p;
				__store_path_set__ = true;
			}
		}

		void MemoryBundleSet::sync() throw ()
		{
			// store the bundle set to disk
			store();
		}

		void MemoryBundleSet::store()
		{
			// abort if the store path is not set
			if (!MemoryBundleSet::__store_path_set__) return;

			// abort if the name is not set
			if (_name.length() == 0) return;

			// create directory, if it does not existt
			if (!__store_path__.exists())
			ibrcommon::File::createDirectory(__store_path__);

			// delete file for bundles, if it exists
			std::stringstream ss; ss << __store_path__.getPath() << "/" << _name;
			ibrcommon::File path_bundles(ss.str().c_str());
			if(path_bundles.exists())
			path_bundles.remove();

			// open file
			std::ofstream output_file;
			output_file.open(path_bundles.getPath().c_str());

			// write number of bundles
			output_file << _bundles.size();

			bundle_set::iterator iter = _bundles.begin();
			while( iter != _bundles.end())
			{
				output_file << (*iter++);
			}

			output_file.close();
		}

		void MemoryBundleSet::restore()
		{
			// abort if the store path is not set
			if (!MemoryBundleSet::__store_path_set__) return;

			std::stringstream ss; ss << __store_path__.getPath() << "/" << _name;
			ibrcommon::File path_bundles(ss.str().c_str());

			// abort if storage files does not exist
			if (!path_bundles.exists()) return;

			std::ifstream input_file;
			input_file.open(ss.str().c_str());

			// read number of bundles
			size_t num_bundles;
			input_file >> num_bundles;
			size_t i = 0;
			while (( i < num_bundles) && input_file.good())
			{
				MetaBundle b;
				input_file >> b;
				add(b);
				i++;
			}

			input_file.close();
		}
	} /* namespace data */
} /* namespace dtn */
