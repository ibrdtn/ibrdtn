/*
 * UDPConvergenceLayer.h
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

#ifndef UDPCONVERGENCELAYER_H_
#define UDPCONVERGENCELAYER_H_

#include "Component.h"
#include "net/ConvergenceLayer.h"
#include <ibrcommon/Exceptions.h>
#include "net/DiscoveryBeaconHandler.h"
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/net/socket.h>
#include <ibrcommon/net/vsocket.h>
#include <ibrcommon/link/LinkManager.h>


namespace dtn
{
	namespace net
	{
		/**
		 * This class implement a ConvergenceLayer for UDP/IP.
		 * Each bundle is sent in exact one UDP datagram.
		 */
		class UDPConvergenceLayer : public ConvergenceLayer, public dtn::daemon::IndependentComponent, public DiscoveryBeaconHandler, public ibrcommon::LinkManager::EventCallback
		{
		public:
			/**
			 * Constructor
			 * @param[in] bind_addr The address to bind.
			 * @param[in] port The udp port to use.
			 * @param[in] broadcast If true, the broadcast feature for this socket is enabled.
			 * @param[in] mtu The maximum bundle size.
			 */
			UDPConvergenceLayer(ibrcommon::vinterface net, int port, dtn::data::Length mtu = 1280);

			/**
			 * Desktruktor
			 */
			virtual ~UDPConvergenceLayer();

			/**
			 * this method updates the given values
			 */
			void onUpdateBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &announcement)
				noexcept (false);

			dtn::core::Node::Protocol getDiscoveryProtocol() const;

			void queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			void eventNotify(const ibrcommon::LinkEvent &evt);

			virtual void resetStats();

			virtual void getStats(ConvergenceLayer::stats_data &data) const;

		protected:
			virtual void componentUp() noexcept;
			virtual void componentRun() noexcept;
			virtual void componentDown() noexcept;
			void __cancellation() noexcept;

		private:
			void receive(dtn::data::Bundle&, dtn::data::EID &sender) noexcept (false);
			void send(const ibrcommon::vaddress &addr, const std::string &data) noexcept (false);

			ibrcommon::vsocket _vsocket;
			ibrcommon::vinterface _net;
			int _port;

			static const int DEFAULT_PORT;

			dtn::data::Length m_maxmsgsize;

			ibrcommon::Mutex m_writelock;
			ibrcommon::Mutex m_readlock;

			bool _running;

			// stats variables
			size_t _stats_in;
			size_t _stats_out;
		};
	}
}

#endif /*UDPCONVERGENCELAYER_H_*/
