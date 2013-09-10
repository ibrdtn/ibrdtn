/*
 * BundleSetImpl.cpp
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
 *  Created on: Apr 5, 2013
 *
 *  only implements destructor
 */

#include "ibrdtn/data/BundleSetImpl.h"

namespace dtn
{
	namespace data
	{
		BundleSetImpl::~BundleSetImpl()
		{

		}
		BundleSetImpl::ExpiringBundle::ExpiringBundle(const MetaBundle &b) : bundle(b)
		{ }

		BundleSetImpl::ExpiringBundle::~ExpiringBundle()
		{ }

		bool BundleSetImpl::ExpiringBundle::operator!=(const ExpiringBundle& other) const
		{
			return !(other == *this);
		}

		bool BundleSetImpl::ExpiringBundle::operator==(const ExpiringBundle& other) const
		{
			return (other.bundle == this->bundle);
		}

		bool BundleSetImpl::ExpiringBundle::operator<(const ExpiringBundle& other) const
		{
			if (bundle.expiretime < other.bundle.expiretime) return true;
			if (bundle.expiretime != other.bundle.expiretime) return false;

			if (bundle < other.bundle) return true;

			return false;
		}

		bool BundleSetImpl::ExpiringBundle::operator>(const ExpiringBundle& other) const
		{
			return !(((*this) < other) || ((*this) == other));
		}
}
}



