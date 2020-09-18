/*
 * WifiP2PManager.cpp
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

#include "net/WifiP2PManager.h"
#include <ibrcommon/Logger.h>
#include "core/BundleCore.h"
#include <unistd.h>

namespace dtn
{
	namespace net
	{
		const std::string WifiP2PManager::TAG = "WifiP2PManager";

		//TODO How to do exception handling (try-catch) in initialization list
		WifiP2PManager::WifiP2PManager(const std::string &ctrlpath)
		 : _running(false), _timeout(120), _priority(10),
		   ce(ctrlpath, dtn::core::BundleCore::local.getString(), *this, *this)
		{
			ce.setTime_ST_SCAN(20);
		}

		WifiP2PManager::~WifiP2PManager()
		{
			// wait until the componentRun() method is finished
			join();
		}

		void WifiP2PManager::__cancellation() noexcept
		{
		}

		void WifiP2PManager::componentUp() noexcept
		{
			IBRCOMMON_LOGGER_TAG(WifiP2PManager::TAG, info) << "initialized" << IBRCOMMON_LOGGER_ENDL;

			// set the running variable to true
			_running = true;

			dtn::core::BundleCore::getInstance().getConnectionManager().add(this);
		}

		void WifiP2PManager::componentRun() noexcept
		{
			while (_running) {
				try {
					ce.addService("IBRDTN");
					ce.run();
				} catch (wifip2p::SupplicantHandleException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
							<< "CoreEngine is not able to successfully communicate with SupplicantHandle. "
							<< "Exception raised: " << ex.what()
							<< IBRCOMMON_LOGGER_ENDL;
					ibrcommon::Thread::sleep(2000);
				}
			}

		}

		void WifiP2PManager::componentDown() noexcept
		{

			// iterates over all connections being established
			list<wifip2p::Connection>::iterator conn_it = connections.begin();

			for (; conn_it != connections.end(); ++conn_it) {
				// remove the respective connection from the local list structure.
				try {
					ce.disconnect(*conn_it);
				} catch (wifip2p::SupplicantHandleException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
										<< "could not remove " << conn_it->getNetworkIntf().getName()
										<< " due to some exception: " << ex.what()
										<< IBRCOMMON_LOGGER_ENDL;
				}
			}

			dtn::core::BundleCore::getInstance().getConnectionManager().remove(this);

			IBRCOMMON_LOGGER_TAG(WifiP2PManager::TAG, info) << "stopped" << IBRCOMMON_LOGGER_ENDL;

			// abort the main loop
			_running = false;

			ce.stop();

		}

		const std::string WifiP2PManager::getName() const
		{
			return WifiP2PManager::TAG;
		}

		dtn::core::Node::Protocol WifiP2PManager::getProtocol() const
		{
			return dtn::core::Node::CONN_P2P_WIFI;
		}

		void WifiP2PManager::connect(const dtn::core::Node::URI &uri)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10) << "connect request to " << uri.value << IBRCOMMON_LOGGER_ENDL;

			wifip2p::Peer p(uri.value);

			list<wifip2p::Peer>::iterator peer_it = peers.begin();
			bool contained = false;

			for (; peer_it != peers.end(); ++peer_it) {
				if (*peer_it == p) {
					contained = true;
					break;
				} else {
					continue;
				}
			}

			if (contained) {
				try {
					ce.connect(p);
				} catch (wifip2p::SupplicantHandleException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
								<< "could not connect to peer " << p.getName()
								<< "@" << p.getMacAddr()
								<< " due to some exception: " << ex.what()
								<< IBRCOMMON_LOGGER_ENDL;
				}
			} else {
				IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
							<< "could not connect to peer " << p.getName()
							<< "@" << p.getMacAddr()
							<< "; peer not avaialable" << IBRCOMMON_LOGGER_ENDL;
			}

		}

		void WifiP2PManager::disconnect(const dtn::core::Node::URI &uri)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10) << "disconnect request from " << uri.value << IBRCOMMON_LOGGER_ENDL;

			wifip2p::Peer p(uri.value);

			try {
				ce.disconnect(p);
			} catch (wifip2p::SupplicantHandleException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
						<< "could not disconnect from peer " << p.getName()
						<< "@" << p.getMacAddr()
						<< " due to some exception: " << ex.what()
						<< IBRCOMMON_LOGGER_ENDL;
			}
		}

		void WifiP2PManager::peerFound(const wifip2p::Peer &peer) {
			// create an EID for the discovered node
			const dtn::data::EID remote_peer_eid(peer.getName());

			std::cout << "Found peer " << peer.getName() << " reported as discovered." << std::endl;

			list<wifip2p::Peer>::iterator peer_it = peers.begin();
			bool contained = false;

			for (; peer_it != peers.end(); ++peer_it) {
				if (*peer_it == peer) {
					contained = true;
					break;
				} else {
					continue;
				}
			}

			if (!contained) {

				std::cout << "Peer discovered as new peer." << std::endl;

				peers.push_back(peer);

				// create a p2p uri for the discovered node
				const dtn::core::Node::URI uri(dtn::core::Node::NODE_P2P_DIALUP, this->getProtocol(), peer.getMacAddr(), _timeout, _priority);

				// announce the discovered peer
				fireDiscovered(remote_peer_eid, uri);

			} else {

				std::cout << "Peer is still available." << std::endl;

				// create a p2p uri for the discovered node
				const dtn::core::Node::URI uri(dtn::core::Node::NODE_P2P_DIALUP, this->getProtocol(), peer.getMacAddr(), _timeout, _priority);

				// announce the discovered peer
				fireDiscovered(remote_peer_eid, uri);

			}


		}

		void WifiP2PManager::connectionRequest(const wifip2p::Peer &peer) {

			list<wifip2p::Connection>::iterator conn_it = connections.begin();
			bool contained = false;

			for (; conn_it != connections.end(); ++conn_it) {
				if (conn_it->getPeer() == peer) {
					contained = true;
					break;
				} else {
					continue;
				}
			}

			if (!contained) {
				try {
					ce.connect(peer);
				} catch (wifip2p::SupplicantHandleException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
								<< "could not connect to peer " << peer.getName()
								<< "@" << peer.getMacAddr()
								<< " due to some exception: " << ex.what()
								<< IBRCOMMON_LOGGER_ENDL;
				}
			} else {
				IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
							<< "peer " << peer.getName() << "@" << peer.getMacAddr()
							<< " already connected through p2p group interface "
							<< conn_it->getNetworkIntf().getName() << IBRCOMMON_LOGGER_ENDL;
			}

		}

		void WifiP2PManager::connectionEstablished(const wifip2p::Connection &conn) {

			// create a new interface object
			const ibrcommon::vinterface iface(conn.getNetworkIntf().getName());

			std::cout << "Will fire interface " << conn.getNetworkIntf().getName()<< " up." << std::endl;

			// push the actually announced connection to the local private list<Connection>
			connections.push_back(conn);

			ibrcommon::Thread::sleep(2000);

			// announce the new p2p interface
			fireInterfaceUp(iface);

		}

		void WifiP2PManager::connectionLost(const wifip2p::Connection &conn) {

			// create a new interface object
			const ibrcommon::vinterface iface(conn.getNetworkIntf().getName());

			std::cout << "Connection at interface " << conn.getNetworkIntf().getName()
					<< " lost. Will fire interface down." << std::endl;

			list<wifip2p::Connection>::iterator conn_it = connections.begin();

			// remove the respective connection from the local list structure.
			for (; conn_it != connections.end(); ++conn_it) {
				if (conn_it->getNetworkIntf() == conn.getNetworkIntf()) {
					connections.erase(conn_it++);
					std::cout << "Connection removed from being locally registered." << std::endl;
				}
			}

			try {
				ce.disconnect(conn);
			} catch (wifip2p::SupplicantHandleException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG, 10)
								<< "could not remove " << conn.getNetworkIntf().getName()
								<< " due to some exception: " << ex.what()
								<< IBRCOMMON_LOGGER_ENDL;
			}

			// de-announce the p2p interface
			fireInterfaceDown(iface);

		}

		void WifiP2PManager::log(const std::string &tag, const std::string &msg)
		{
			IBRCOMMON_LOGGER_TAG(WifiP2PManager::TAG + "[" + tag + "]", notice) << msg << IBRCOMMON_LOGGER_ENDL;
		}

		void WifiP2PManager::log_err(const std::string &tag, const std::string &msg)
		{
			IBRCOMMON_LOGGER_TAG(WifiP2PManager::TAG + "[" + tag + "]", error) << msg << IBRCOMMON_LOGGER_ENDL;
		}

		void WifiP2PManager::log_debug(int debug, const std::string &tag, const std::string &msg)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(WifiP2PManager::TAG + "[" + tag + "]", debug) << msg << IBRCOMMON_LOGGER_ENDL;
		}
	} /* namespace net */
} /* namespace dtn */
