/*
 * BundleSetFactory.cpp
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

#include "ibrdtn/data/BundleSetFactory.h"
#include "ibrdtn/data/MemoryBundleSet.h"

namespace dtn
{
	namespace data
	{
		dtn::data::BundleSetFactory* BundleSetFactory::bundleSetFactory = NULL;

		BundleSetFactory::~BundleSetFactory(){}

		BundleSetImpl* BundleSetFactory::create(BundleSet::Listener* listener, Size bf_size){

			if(BundleSetFactory::bundleSetFactory != NULL)
				return bundleSetFactory->createBundleSet(listener,bf_size);

			return new MemoryBundleSet(listener,bf_size); //standard: MemoryBundleSet

		}

		BundleSetImpl* BundleSetFactory::create(const std::string &name, BundleSet::Listener* listener, Size bf_size){

			if(BundleSetFactory::bundleSetFactory != NULL)
				return bundleSetFactory->createBundleSet(name,listener,bf_size);


			return new MemoryBundleSet(listener,bf_size); //MemoryBundleSet does not support naming
		}
	} /* namespace data */
} /* namespace dtn */


