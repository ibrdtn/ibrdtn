/*
 * WifiP2PManager.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 * Written-by: Niels Wuerzbach <wuerzb@ibr.cs.tu-bs.de>
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

#ifndef WIFIP2PMANAGER_H_
#define WIFIP2PMANAGER_H_

#include "Component.h"
#include "net/P2PDialupExtension.h"
#include <ibrcommon/net/vsocket.h>

#include <wifip2p/CoreEngine.h>
#include <wifip2p/WifiP2PInterface.h>
#include <wifip2p/Logger.h>
#include <list>

namespace dtn
{
	namespace net
	{
		class WifiP2PManager : public dtn::daemon::IndependentComponent, public dtn::net::P2PDialupExtension, public wifip2p::WifiP2PInterface, public wifip2p::Logger
		{
			static const std::string TAG;

		public:
			WifiP2PManager(const std::string &ctrlpath);
			virtual ~WifiP2PManager();

			/**
			 * @see Component::__cancellation()
			 */
			virtual void __cancellation() noexcept;

			/**
			 * @see Component::componentUp()
			 */
			virtual void componentUp() noexcept;

			/**
			 * @see Component::componentRun()
			 */
			virtual void componentRun() noexcept;

			/**
			 * @see Component::componentDown()
			 */
			virtual void componentDown() noexcept;

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			/**
			 * @see P2PDialExtension::getProtocol()
			 */
			virtual dtn::core::Node::Protocol getProtocol() const;

			/**
			 * @see P2PDialExtension::connect()
			 */
			virtual void connect(const dtn::core::Node::URI &uri);

			/**
			 * @see P2PDialExtension::disconnect()
			 */
			virtual void disconnect(const dtn::core::Node::URI &uri);


			/**
			 * @see wifip2p::WifiP2PInterface::peerFound(wifip2p::Peer peer)
			 */
			virtual void peerFound(const wifip2p::Peer &peer);

			/**
			 * @see wifip2p::WifiP2PInterface::connectionRequest(wifip2p::Peer peer)
			 */
			virtual void connectionRequest(const wifip2p::Peer &peer);

			/**
			 * @see wifip2p::WifiP2PInterface::connectionEstablished(wifip2p::Connection conn)
			 */
			virtual void connectionEstablished(const wifip2p::Connection &conn);

			/**
			 * @see wifip2p::WifiP2PInterface::connectionLost(wifip2p::Connection conn)
			 */
			virtual void connectionLost(const wifip2p::Connection &conn);

			/**
			 * @see wifip2p::Logger::log(std::string tag, std::string msg)
			 */
			virtual void log(const std::string &tag, const std::string &msg);

			/**
			 * @see wifip2p::Logger::log_err(std::string tag, std::string msg)
			 */
			virtual void log_err(const std::string &tag, const std::string &msg);

			/**
			 * @see wifip2p::Logger::log_debug(int debug, std::string tag, std::string msg)
			 */
			virtual void log_debug(int debug, const std::string &tag, const std::string &msg);

		private:
			bool _running;
			size_t _timeout;
			int _priority;

			wifip2p::CoreEngine ce;

			list<wifip2p::Peer> peers;
			list<wifip2p::Connection> connections;
		};
	} /* namespace net */
} /* namespace dtn */
#endif /* WIFIP2PMANAGER_H_ */
