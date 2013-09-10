/*
 * BundleSetFactory.h
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
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
 *  Created on: May 7, 2013
 */

#ifndef BUNDLESETFACTORY_H_
#define BUNDLESETFACTORY_H_

#include "ibrdtn/data/BundleSet.h"
#include "ibrdtn/data/BundleSetImpl.h"
#include "ibrdtn/data/Number.h"

namespace dtn
{
	namespace data
	{
		class BundleSetFactory {
			public:
				virtual ~BundleSetFactory();

				/*
				 * create new BundleSet, based on inner BundleSetFactory
				 */
				static BundleSetImpl* create(BundleSet::Listener* listener, Size bf_size);


				static BundleSetImpl* create(const std::string &name, BundleSet::Listener* listener, Size bf_size);

				/*
				 * the BundleSetFactory, which creates the bundles
				 */
				static dtn::data::BundleSetFactory* bundleSetFactory;

			protected:
				virtual BundleSetImpl* createBundleSet(BundleSet::Listener* listener, Size bf_size) = 0;
				virtual BundleSetImpl* createBundleSet(const std::string &name, BundleSet::Listener* listener, Size bf_size) = 0;
		};

	} /* namespace data */
} /* namespace dtn */

#endif /* BUNDLESETFACTORY_H_ */
