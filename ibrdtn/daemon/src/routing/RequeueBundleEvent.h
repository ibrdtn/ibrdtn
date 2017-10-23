/*
 * RequeueBundleEvent.h
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

#ifndef REQUEUEBUNDLEEVENT_H_
#define REQUEUEBUNDLEEVENT_H_

#include "core/Node.h"
#include "core/Event.h"
#include "ibrdtn/data/BundleID.h"
#include "ibrdtn/data/EID.h"

namespace dtn
{
	namespace routing
	{
		class RequeueBundleEvent : public dtn::core::Event
		{
		public:
			virtual ~RequeueBundleEvent();

			const std::string getName() const;

			std::string getMessage() const;

			const dtn::data::EID& getPeer() const;

			const dtn::data::BundleID& getBundle() const;

			dtn::core::Node::Protocol getProtocol() const;

			static void raise(const dtn::data::EID peer, const dtn::data::BundleID &id, dtn::core::Node::Protocol p);

		private:
			RequeueBundleEvent(const dtn::data::EID peer, const dtn::data::BundleID &id, dtn::core::Node::Protocol p);

			dtn::data::EID _peer;
			dtn::data::BundleID _bundle;
			dtn::core::Node::Protocol _protocol;
		};
	}
}


#endif /* REQUEUEBUNDLEEVENT_H_ */
