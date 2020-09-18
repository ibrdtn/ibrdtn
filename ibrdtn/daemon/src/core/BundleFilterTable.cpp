/*
 * BundleFilterTable.cpp
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <jm@m-network.de>
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

#include "core/BundleFilterTable.h"

namespace dtn
{
	namespace core
	{
		BundleFilterTable::BundleFilterTable()
		{
		}

		BundleFilterTable::~BundleFilterTable()
		{
			// clean-up filter in chain
			for (chain::const_iterator it = _chain.begin(); it != _chain.end(); ++it)
			{
				delete (*it);
			}
		}

		void BundleFilterTable::append(BundleFilter *filter)
		{
			_chain.push_back(filter);
		}

		void BundleFilterTable::insert(unsigned int position, BundleFilter *filter)
		{
			chain::iterator it = _chain.begin();

			// move iterator to the right position
			for (unsigned int i = 0; i < position && it != _chain.end(); ++it, ++i);

			// insert before the position
			_chain.insert(it, filter);
		}

		void BundleFilterTable::clear()
		{
			// clean-up filter in chain
			for (chain::const_iterator it = _chain.begin(); it != _chain.end(); ++it)
			{
				delete (*it);
			}
			_chain.clear();
		}

		BundleFilter::ACTION BundleFilterTable::evaluate(const FilterContext &context) const noexcept
		{
			for (chain::const_iterator it = _chain.begin(); it != _chain.end(); ++it)
			{
				const BundleFilter &f = (**it);
				const BundleFilter::ACTION ret = f.evaluate(context);
				if (ret != BundleFilter::PASS && ret != BundleFilter::SKIP) return ret;
			}
			return ACCEPT;
		}

		BundleFilter::ACTION BundleFilterTable::filter(const FilterContext &context, dtn::data::Bundle &bundle) const noexcept
		{
			for (chain::const_iterator it = _chain.begin(); it != _chain.end(); ++it)
			{
				const BundleFilter &f = (**it);
				const BundleFilter::ACTION ret = f.filter(context, bundle);
				if (ret != BundleFilter::PASS && ret != BundleFilter::SKIP) return ret;
			}
			return ACCEPT;
		}
	} /* namespace core */
} /* namespace dtn */
