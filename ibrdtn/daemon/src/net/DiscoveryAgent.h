/*
 * DiscoveryAgent.h
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

#ifndef DISCOVERYAGENT_H_
#define DISCOVERYAGENT_H_

#include "Configuration.h"
#include "Component.h"
#include "core/EventReceiver.h"
#include "core/Node.h"
#include "core/TimeEvent.h"
#include "core/GlobalEvent.h"
#include "net/DiscoveryBeacon.h"
#include "net/DiscoveryBeaconHandler.h"
#include "net/DiscoveryService.h"

#include <ibrdtn/data/Number.h>
#include <ibrcommon/net/vinterface.h>

#include <list>

namespace dtn
{
	namespace net
	{
		class DiscoveryAgent : public dtn::core::EventReceiver<dtn::core::TimeEvent>, public dtn::core::EventReceiver<dtn::core::GlobalEvent>, public dtn::daemon::IntegratedComponent
		{
		public:
			DiscoveryAgent();
			virtual ~DiscoveryAgent();

			/**
			 * method to receive global events
			 */
			void raiseEvent(const dtn::core::TimeEvent &evt) throw ();
			void raiseEvent(const dtn::core::GlobalEvent &evt) throw ();

			void onBeaconReceived(const DiscoveryBeacon &beacon);

			void registerService(const ibrcommon::vinterface &iface, dtn::net::DiscoveryBeaconHandler *handler);
			void registerService(dtn::net::DiscoveryBeaconHandler *handler);

			void unregisterService(const ibrcommon::vinterface &iface, const dtn::net::DiscoveryBeaconHandler *handler);
			void unregisterService(const dtn::net::DiscoveryBeaconHandler *handler);

			DiscoveryBeacon obtainBeacon() const;

		protected:
			virtual void componentUp() throw ();
			virtual void componentDown() throw ();

			virtual const std::string getName() const;

		private:
			void onAdvertise();

			const dtn::daemon::Configuration::Discovery &_config;

			bool _enabled;
			uint16_t _sn;
			dtn::data::Timestamp _adv_next;
			unsigned int _beacon_period;

			ibrcommon::Mutex _provider_lock;

			typedef std::list<dtn::net::DiscoveryBeaconHandler*> handler_list;
			typedef std::map<ibrcommon::vinterface, handler_list> handler_map;
			typedef std::pair<ibrcommon::vinterface, handler_list> handler_map_entry;

			handler_map _providers;
			handler_list _default_providers;
		};
	}
}

#endif /* DISCOVERYAGENT_H_ */
