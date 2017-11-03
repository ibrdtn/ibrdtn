/*
 * IPNDAgent.h
 *
 * IPND is based on the Internet-Draft for DTN-IPND.
 *
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

#ifndef IPNDAGENT_H_
#define IPNDAGENT_H_

#include "Component.h"
#include "net/DiscoveryBeaconHandler.h"
#include "net/P2PDialupEvent.h"
#include "core/EventReceiver.h"
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/net/vsocket.h>
#include <ibrcommon/link/LinkManager.h>
#include <ibrcommon/thread/Mutex.h>
#include <list>
#include <map>

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		class IPNDAgent : public dtn::core::EventReceiver<dtn::net::P2PDialupEvent>, public dtn::daemon::IndependentComponent, public ibrcommon::LinkManager::EventCallback, public DiscoveryBeaconHandler
		{
			static const std::string TAG;

		public:
			IPNDAgent(int port);
			virtual ~IPNDAgent();

			void add(const ibrcommon::vaddress &address);
			void bind(const ibrcommon::vinterface &net);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			void eventNotify(const ibrcommon::LinkEvent &evt);

			/**
			 * @see EventReceiver::raiseEvent()
			 */
			void raiseEvent(const dtn::net::P2PDialupEvent &evt) throw ();

			/**
			 * This method is called by the DiscoveryAgent every time a beacon is ready for advertisement
			 */
			void onAdvertiseBeacon(const ibrcommon::vinterface &iface, const DiscoveryBeacon &beacon) throw ();

		protected:
			virtual void componentRun() throw ();
			virtual void componentUp() throw ();
			virtual void componentDown() throw ();
			void __cancellation() throw ();

		private:
			void join(const ibrcommon::vinterface &iface) throw ();
			void leave(const ibrcommon::vinterface &iface) throw ();

			void join(const ibrcommon::vinterface &iface, const ibrcommon::vaddress &addr) throw ();
			void leave(const ibrcommon::vinterface &iface, const ibrcommon::vaddress &addr) throw ();

			void send(const DiscoveryBeacon &a, const ibrcommon::vinterface &iface, const ibrcommon::vaddress &addr);

			ibrcommon::vsocket _socket;
			bool _state;
			int _port;

			std::set<ibrcommon::vaddress> _destinations;

			ibrcommon::Mutex _interface_lock;
			std::set<ibrcommon::vinterface> _interfaces;
		};
	}
}

#endif /* IPNDAGENT_H_ */
