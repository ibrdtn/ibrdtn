/*
 * BundleExpiredEvent.h
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

#ifndef BUNDLEEXPIREDEVENT_H_
#define BUNDLEEXPIREDEVENT_H_

#include "core/Event.h"
#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/data/BundleID.h"
#include "ibrdtn/data/EID.h"

namespace dtn
{
	namespace core
	{
		class BundleExpiredEvent : public dtn::core::Event
		{
		public:
			virtual ~BundleExpiredEvent();

			const std::string getName() const;

			std::string getMessage() const;

			const dtn::data::BundleID& getBundle() const;

			static void raise(const dtn::data::Bundle &bundle);
			static void raise(const dtn::data::BundleID &bundle);

		private:
			BundleExpiredEvent(const dtn::data::Bundle &bundle);
			BundleExpiredEvent(const dtn::data::BundleID &bundle);

			const dtn::data::BundleID _bundle;
		};
	}
}

#endif /* BUNDLEEXPIREDEVENT_H_ */
