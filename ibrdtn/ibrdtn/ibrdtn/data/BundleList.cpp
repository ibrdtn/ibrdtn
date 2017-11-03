/*
 * BundleList.cpp
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

#include "ibrdtn/data/BundleList.h"
#include <algorithm>

namespace dtn
{
	namespace data
	{
		BundleList::Listener::~Listener() { }

		BundleList::BundleList(BundleList::Listener *listener)
		 : _listener(listener)
		{ }

		BundleList::~BundleList()
		{ }

		void BundleList::add(const dtn::data::MetaBundle &bundle) throw ()
		{
			// insert the bundle to the public list
			std::pair<iterator,bool> ret = _meta_bundles.insert(bundle);

			if (ret.second) {
				ExpiringBundle exb(*ret.first);
				_bundles.insert(exb);
			}
		}

		void BundleList::remove(const dtn::data::MetaBundle &bundle) throw ()
		{
			// search for the bundle in the public list
			const std::set<dtn::data::MetaBundle>::iterator it = _meta_bundles.find(bundle);

			// only if the bundle is in the public list
			if (it != _meta_bundles.end())
			{
				// remove the bundle from the set of expiring bundles
				_bundles.erase(*it);

				// remove the bundle from the public list
				_meta_bundles.erase(it);
			}
		}

		void BundleList::clear() throw ()
		{
			_bundles.clear();
			_meta_bundles.clear();
		}

		void BundleList::expire(const Timestamp &timestamp) throw ()
		{
			// we can not expire bundles if we have no idea of time
			if (timestamp == 0) return;

			std::set<ExpiringBundle>::iterator iter = _bundles.begin();

			while (iter != _bundles.end())
			{
				const ExpiringBundle &b = (*iter);

				if ( b.bundle.expiretime >= timestamp ) break;

				// raise expired event
				if (_listener != NULL)
					_listener->eventBundleExpired( b.bundle );

				// remove this item in public list
				_meta_bundles.erase( b.bundle );

				// remove this item in private list
				_bundles.erase( iter++ );
			}
		}

		BundleList::ExpiringBundle::ExpiringBundle(const MetaBundle &b)
		 : bundle(b)
		{ }

		BundleList::ExpiringBundle::~ExpiringBundle()
		{ }

		bool BundleList::ExpiringBundle::operator!=(const ExpiringBundle& other) const
		{
			return !(other == *this);
		}

		bool BundleList::ExpiringBundle::operator==(const ExpiringBundle& other) const
		{
			return (other.bundle == this->bundle);
		}

		bool BundleList::ExpiringBundle::operator<(const ExpiringBundle& other) const
		{
			if (bundle.expiretime < other.bundle.expiretime) return true;
			if (bundle.expiretime != other.bundle.expiretime) return false;

			if (bundle < other.bundle) return true;

			return false;
		}

		bool BundleList::ExpiringBundle::operator>(const ExpiringBundle& other) const
		{
			return !(((*this) < other) || ((*this) == other));
		}
	}
}
