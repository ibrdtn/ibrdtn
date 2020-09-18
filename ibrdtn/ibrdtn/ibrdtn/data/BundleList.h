/*
 * BundleList.h
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

#ifndef BUNDLELIST_H_
#define BUNDLELIST_H_

#include <ibrdtn/data/Number.h>
#include <ibrdtn/data/MetaBundle.h>
#include <set>

namespace dtn
{
	namespace data
	{
		class BundleList
		{
		public:
			class Listener {
			public:
				virtual ~Listener() = 0;
				virtual void eventBundleExpired(const dtn::data::MetaBundle&) noexcept = 0;
			};

			BundleList(BundleList::Listener *listener = NULL);
			virtual ~BundleList();

			virtual void add(const dtn::data::MetaBundle &bundle) noexcept;
			virtual void remove(const dtn::data::MetaBundle &bundle) noexcept;
			virtual void clear() noexcept;

			virtual void expire(const Timestamp &timestamp) noexcept;

			typedef std::set<dtn::data::MetaBundle> meta_set;
			typedef meta_set::iterator iterator;
			typedef meta_set::const_iterator const_iterator;

			iterator begin() { return _meta_bundles.begin(); }
			iterator end() { return _meta_bundles.end(); }

			const_iterator begin() const { return _meta_bundles.begin(); }
			const_iterator end() const { return _meta_bundles.end(); }

			template<class T>
			iterator find(const T &b) { return _meta_bundles.find(b); }

			template<class T>
			const_iterator find(const T &b) const { return _meta_bundles.find(b); }

			bool empty() const { return _meta_bundles.empty(); }
			Size size() const { return _meta_bundles.size(); }

		private:
			class ExpiringBundle
			{
			public:
				ExpiringBundle(const MetaBundle &b);
				virtual ~ExpiringBundle();

				bool operator!=(const ExpiringBundle& other) const;
				bool operator==(const ExpiringBundle& other) const;
				bool operator<(const ExpiringBundle& other) const;
				bool operator>(const ExpiringBundle& other) const;

				const MetaBundle &bundle;
			};

			std::set<ExpiringBundle> _bundles;
			std::set<dtn::data::MetaBundle> _meta_bundles;

			BundleList::Listener *_listener;
		};

	}
}

#endif /* BUNDLELIST_H_ */
