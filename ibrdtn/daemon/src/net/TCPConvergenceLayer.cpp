/*
 * TCPConvergenceLayer.cpp
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

#include "Configuration.h"

#include "net/TCPConvergenceLayer.h"
#include "net/ConnectionEvent.h"
#include "net/DiscoveryAgent.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/net/socket.h>
#include <ibrcommon/Logger.h>

#include <streambuf>
#include <functional>
#include <list>
#include <algorithm>

#ifdef WITH_TLS
#include <ibrcommon/ssl/TLSStream.h>
#endif

namespace dtn
{
	namespace net
	{
		/*
		 * class TCPConvergenceLayer
		 */
		const std::string TCPConvergenceLayer::TAG = "TCPConvergenceLayer";

		const int TCPConvergenceLayer::DEFAULT_PORT = 4556;

		TCPConvergenceLayer::TCPConvergenceLayer()
		 : _vsocket_state(false), _any_port(0), _stats_in(0), _stats_out(0),
		   _keepalive_timeout( dtn::daemon::Configuration::getInstance().getNetwork().getKeepaliveInterval() )
		{
		}

		TCPConvergenceLayer::~TCPConvergenceLayer()
		{
			// unsubscribe to NetLink events
			ibrcommon::LinkManager::getInstance().removeEventListener(this);

			// un-register as discovery handler
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(this);

			join();

			// delete all sockets
			_vsocket.destroy();
		}

		void TCPConvergenceLayer::add(const ibrcommon::vinterface &net, int port) throw ()
		{
			// do not allow any futher binding if we already bound to any interface
			if (_any_port > 0) return;

			if (net.isLoopback()) {
				// bind to v6 loopback address if supported
				if (ibrcommon::basesocket::hasSupport(AF_INET6)) {
					ibrcommon::vaddress addr6(ibrcommon::vaddress::VADDR_LOCALHOST, port, AF_INET6);
					_vsocket.add(new ibrcommon::tcpserversocket(addr6));
				}

				// bind to v4 loopback address
				ibrcommon::vaddress addr4(ibrcommon::vaddress::VADDR_LOCALHOST, port, AF_INET);
				_vsocket.add(new ibrcommon::tcpserversocket(addr4));
			} else {
				listen(net, port);
			}
		}

		void TCPConvergenceLayer::listen(const ibrcommon::vinterface &net, int port) throw ()
		{
			try {
				// add the new interface to internal data-structures
				{
					ibrcommon::MutexLock l(_interface_lock);

					// only add the interface once
					if (_interfaces.find(net) != _interfaces.end()) return;

					// store the new interface in the list of interfaces
					_interfaces.insert(net);
				}

				// subscribe to NetLink events on our interfaces
				ibrcommon::LinkManager::getInstance().addEventListener(net, this);

				// register as discovery handler for this interface
				dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(net, this);

				// store port of the interface
				{
					ibrcommon::MutexLock l(_portmap_lock);
					_portmap[net] = port;
				}

				if (net.isAny())
				{
					// Bind once to ANY interface
					_vsocket.add(new ibrcommon::tcpserversocket(port));
					_any_port = port;
					return;
				}

				bindSockets(net, port);
			} catch (const ibrcommon::vinterface::interface_not_set &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConvergenceLayer::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void TCPConvergenceLayer::bindSockets(const ibrcommon::vinterface &net, const int port) throw ()
		{
			// create sockets for all addresses on the interface
			// may throw "interface_not_set"
			std::list<ibrcommon::vaddress> addrs = net.getAddresses();

			// convert the port into a string
			std::stringstream ss; ss << port;

			for (std::list<ibrcommon::vaddress>::iterator iter = addrs.begin(); iter != addrs.end(); ++iter) {
				ibrcommon::vaddress &addr = (*iter);

				// handle the addresses according to their family
				// may throw "address_exception"
				try {
					switch (addr.family()) {
						case AF_INET:
						case AF_INET6:
						{
							addr.setService(ss.str());
							ibrcommon::tcpserversocket *sock = new ibrcommon::tcpserversocket(addr);
							if (_vsocket_state) sock->up();
							_vsocket.add(sock, net);

							IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 25) << "bound to " << net.toString() << " (" << addr.toString() << ", family: " << addr.family() << ")" << IBRCOMMON_LOGGER_ENDL;
							break;
						}
						default:
							break;
					}
				} catch (const ibrcommon::vaddress::address_exception &ex) {
					IBRCOMMON_LOGGER_TAG(TCPConvergenceLayer::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				} catch (const ibrcommon::socket_exception &ex) {
					IBRCOMMON_LOGGER_TAG(TCPConvergenceLayer::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}

		void TCPConvergenceLayer::unlisten(const ibrcommon::vinterface &iface) throw ()
		{
			ibrcommon::socketset socks = _vsocket.get(iface);
			for (ibrcommon::socketset::iterator iter = socks.begin(); iter != socks.end(); ++iter) {
				ibrcommon::tcpserversocket *sock = dynamic_cast<ibrcommon::tcpserversocket*>(*iter);
				_vsocket.remove(sock);
				try {
					sock->down();
				} catch (const ibrcommon::socket_exception &ex) {
					IBRCOMMON_LOGGER_TAG(TCPConvergenceLayer::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
				delete sock;
			}

			{
				ibrcommon::MutexLock l(_portmap_lock);
				_portmap.erase(iface);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 25) << "unbound from " << iface.toString() << IBRCOMMON_LOGGER_ENDL;
		}

		dtn::core::Node::Protocol TCPConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_TCPIP;
		}

		void TCPConvergenceLayer::onUpdateBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &beacon) throw (dtn::net::DiscoveryBeaconHandler::NoServiceHereException)
		{
			ibrcommon::MutexLock l(_interface_lock);

			// announce port only if we are bound to any interface
			if (_interfaces.empty() && (_any_port > 0)) {
				std::stringstream service;
				// ... set the port only
				ibrcommon::MutexLock l(_portmap_lock);
				service << "port=" << _portmap[iface] << ";";
				beacon.addService( DiscoveryService(getDiscoveryProtocol(), service.str()));
				return;
			}

			// determine if we should enable crosslayer discovery by sending out our own address
			bool crosslayer = dtn::daemon::Configuration::getInstance().getDiscovery().enableCrosslayer();

			// this marker should set to true if we added an service description
			bool announced = false;

			// search for the matching interface
			for (std::set<ibrcommon::vinterface>::const_iterator it = _interfaces.begin(); it != _interfaces.end(); ++it)
			{
				const ibrcommon::vinterface &it_iface = *it;
				if (it_iface == iface)
				{
					try {
						// check if cross layer discovery is disabled
						if (!crosslayer) throw ibrcommon::Exception("crosslayer discovery disabled!");

						// get all addresses of this interface
						std::list<ibrcommon::vaddress> list = it_iface.getAddresses();

						// if no address is returned... (goto catch block)
						if (list.empty()) throw ibrcommon::Exception("no address found");

						for (std::list<ibrcommon::vaddress>::const_iterator addr_it = list.begin(); addr_it != list.end(); ++addr_it)
						{
							const ibrcommon::vaddress &addr = (*addr_it);

							if (addr.scope() != ibrcommon::vaddress::SCOPE_GLOBAL) continue;

							try {
								// do not announce non-IP addresses
								sa_family_t f = addr.family();
								if ((f != AF_INET) && (f != AF_INET6)) continue;

								std::stringstream service;
								// fill in the ip address
								ibrcommon::MutexLock l(_portmap_lock);
								service << "ip=" << addr.address() << ";port=" << _portmap[iface] << ";";
								beacon.addService( DiscoveryService(getDiscoveryProtocol(), service.str()));

								// set the announce mark
								announced = true;
							} catch (const ibrcommon::vaddress::address_exception &ex) {
								IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 25) << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}
						}
					} catch (const ibrcommon::Exception &ex) {
						// address collection process aborted
						IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 65) << "Address collection aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					};

					// if we still not announced anything...
					if (!announced) {
						std::stringstream service;
						// ... set the port only
						ibrcommon::MutexLock l(_portmap_lock);
						service << "port=" << _portmap[iface] << ";";
						beacon.addService( DiscoveryService(getDiscoveryProtocol(), service.str()));
					}
					return;
				}
			}

			throw dtn::net::DiscoveryBeaconHandler::NoServiceHereException();
		}

		const std::string TCPConvergenceLayer::getName() const
		{
			return TCPConvergenceLayer::TAG;
		}

		void TCPConvergenceLayer::raiseEvent(const dtn::net::P2PDialupEvent &dialup) throw ()
		{
			switch (dialup.type)
			{
				case dtn::net::P2PDialupEvent::INTERFACE_UP:
				{
					// listen to the new interface
					listen(dialup.iface, TCPConvergenceLayer::DEFAULT_PORT);
					break;
				}

				case dtn::net::P2PDialupEvent::INTERFACE_DOWN:
				{
					// check if the interface is bound by us
					{
						ibrcommon::MutexLock l(_interface_lock);

						// only remove the interface if it exists
						if (_interfaces.find(dialup.iface) == _interfaces.end()) return;

						// remove the interface from the stored set
						_interfaces.erase(dialup.iface);
					}

					// un-subscribe to NetLink events on our interfaces
					ibrcommon::LinkManager::getInstance().removeEventListener(dialup.iface, this);

					// un-register as discovery handler for this interface
					dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(dialup.iface, this);

					// remove all sockets on this interface
					unlisten(dialup.iface);
					break;
				}
			}
		}

		void TCPConvergenceLayer::eventNotify(const ibrcommon::LinkEvent &evt)
		{
			// do not do anything if we are bound on all interfaces
			if (_any_port > 0) return;

			{
				ibrcommon::MutexLock l(_interface_lock);
				if (_interfaces.find(evt.getInterface()) == _interfaces.end()) return;
			}

			switch (evt.getAction())
			{
				case ibrcommon::LinkEvent::ACTION_ADDRESS_ADDED:
				{
					ibrcommon::vaddress bindaddr = evt.getAddress();
					// convert the port into a string
					ibrcommon::MutexLock l(_portmap_lock);
					std::stringstream ss; ss << _portmap[evt.getInterface()];
					bindaddr.setService(ss.str());
					ibrcommon::tcpserversocket *sock = new ibrcommon::tcpserversocket(bindaddr);
					try {
						sock->up();
						_vsocket.add(sock, evt.getInterface());
					} catch (const ibrcommon::socket_exception&) {
						delete sock;
					}
					break;
				}

				case ibrcommon::LinkEvent::ACTION_ADDRESS_REMOVED:
				{
					ibrcommon::socketset socks = _vsocket.getAll();
					for (ibrcommon::socketset::iterator iter = socks.begin(); iter != socks.end(); ++iter) {
						ibrcommon::tcpserversocket *sock = dynamic_cast<ibrcommon::tcpserversocket*>(*iter);
						if (sock->get_address().address() == evt.getAddress().address()) {
							_vsocket.remove(sock);
							sock->down();
							delete sock;
							break;
						}
					}
					break;
				}

				case ibrcommon::LinkEvent::ACTION_LINK_UP:
				{
					const ibrcommon::vinterface &iface = evt.getInterface();
					{
						// check if the interface is on the configuration list to listen to.
						ibrcommon::MutexLock l(_interface_lock);
						if (_interfaces.find(iface) == _interfaces.end()) {
							break;
						}
					}
					unsigned int port = TCPConvergenceLayer::DEFAULT_PORT;
					{
						ibrcommon::MutexLock l(_portmap_lock);
						port = _portmap[iface];
					}
					bindSockets(iface, port);
					break;
				}

				case ibrcommon::LinkEvent::ACTION_LINK_DOWN:
				{
					// remove all sockets on this interface
					const ibrcommon::vinterface &iface = evt.getInterface();

					ibrcommon::socketset socks = _vsocket.get(iface);
					for (ibrcommon::socketset::iterator iter = socks.begin(); iter != socks.end(); ++iter) {
						ibrcommon::tcpserversocket *sock = dynamic_cast<ibrcommon::tcpserversocket*>(*iter);
						_vsocket.remove(sock);
						try {
							sock->down();
						} catch (const ibrcommon::socket_exception &ex) {
							IBRCOMMON_LOGGER_TAG(TCPConvergenceLayer::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
						}
						delete sock;
					}

					break;
				}

				default:
					break;
			}
		}

		void TCPConvergenceLayer::open(const dtn::core::Node &n)
		{
			// search for an existing connection
			ibrcommon::MutexLock l(_connections_cond);

			for (std::list<TCPConnection*>::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				TCPConnection &conn = *(*iter);

				if (conn.match(n))
				{
					return;
				}
			}

			try {
				// create a connection
				TCPConnection *conn = new TCPConnection(*this, n, NULL, _keepalive_timeout);

#ifdef WITH_TLS
				// enable TLS Support
				if ( ibrcommon::TLSStream::isInitialized() )
				{
					conn->enableTLS();
				}
#endif

				// raise setup event
				ConnectionEvent::raise(ConnectionEvent::CONNECTION_SETUP, n);

				// add connection as pending
				_connections.push_back( conn );

				// start the ClientHandler (service)
				conn->initialize();

				// signal that there is a new connection
				_connections_cond.signal(true);
			} catch (const ibrcommon::Exception&) {	};

			return;
		}

		void TCPConvergenceLayer::queue(const dtn::core::Node &n, const dtn::net::BundleTransfer &job)
		{
			// search for an existing connection
			ibrcommon::MutexLock l(_connections_cond);

			for (std::list<TCPConnection*>::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				TCPConnection &conn = *(*iter);

				if (conn.match(n))
				{
					conn.queue(job);
					IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 15) << "queued bundle to an existing tcp connection (" << conn.getNode().toString() << ")" << IBRCOMMON_LOGGER_ENDL;

					return;
				}
			}

			try {
				// create a connection
				TCPConnection *conn = new TCPConnection(*this, n, NULL, _keepalive_timeout);

#ifdef WITH_TLS
				// enable TLS Support
				if ( ibrcommon::TLSStream::isInitialized() )
				{
					conn->enableTLS();
				}
#endif

				// raise setup event
				ConnectionEvent::raise(ConnectionEvent::CONNECTION_SETUP, n);

				// add connection as pending
				_connections.push_back( conn );

				// start the ClientHandler (service)
				conn->initialize();

				// queue the bundle
				conn->queue(job);

				// signal that there is a new connection
				_connections_cond.signal(true);

				IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 15) << "queued bundle to an new tcp connection (" << conn->getNode().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
			} catch (const ibrcommon::Exception&) {

			}
		}

		void TCPConvergenceLayer::connectionUp(TCPConnection *conn)
		{
			ibrcommon::MutexLock l(_connections_cond);
			for (std::list<TCPConnection*>::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				if (conn == (*iter))
				{
					// put pending connection to the active connections
					return;
				}
			}

			_connections.push_back( conn );

			// signal that there is a new connection
			_connections_cond.signal(true);

			IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 15) << "tcp connection added (" << conn->getNode().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
		}

		void TCPConvergenceLayer::connectionDown(TCPConnection *conn)
		{
			ibrcommon::MutexLock l(_connections_cond);
			for (std::list<TCPConnection*>::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				if (conn == (*iter))
				{
					_connections.erase(iter);
					IBRCOMMON_LOGGER_DEBUG_TAG(TCPConvergenceLayer::TAG, 15) << "tcp connection removed (" << conn->getNode().toString() << ")" << IBRCOMMON_LOGGER_ENDL;

					// signal that there is a connection less
					_connections_cond.signal(true);
					return;
				}
			}
		}

		void TCPConvergenceLayer::addTrafficIn(size_t amount) throw ()
		{
			ibrcommon::MutexLock l(_stats_lock);
			_stats_in += amount;
		}

		void TCPConvergenceLayer::addTrafficOut(size_t amount) throw ()
		{
			ibrcommon::MutexLock l(_stats_lock);
			_stats_out += amount;
		}

		void TCPConvergenceLayer::componentRun() throw ()
		{
			try {
				while (true)
				{
					ibrcommon::socketset readfds;
					
					// wait for incoming connections
					_vsocket.select(&readfds, NULL, NULL, NULL);

					for (ibrcommon::socketset::iterator iter = readfds.begin(); iter != readfds.end(); ++iter) {
						try {
							// assume that all sockets are serversockets
							ibrcommon::serversocket &sock = dynamic_cast<ibrcommon::serversocket&>(**iter);

							// wait for incoming connections
							ibrcommon::vaddress peeraddr;
							ibrcommon::clientsocket *client = sock.accept(peeraddr);

							// create a EID based on the peer address
							dtn::data::EID source("tcp://" + peeraddr.address() + ":" + peeraddr.service());

							// create a new node object
							dtn::core::Node node(source);

							// add TCP connection
							const std::string uri = "ip=" + peeraddr.address() + ";port=" + peeraddr.service() + ";";
							node.add( dtn::core::Node::URI(Node::NODE_CONNECTED, Node::CONN_TCPIP, uri, 0, 10) );

							// create a new TCPConnection and return the pointer
							TCPConnection *obj = new TCPConnection(*this, node, client, _keepalive_timeout);

#ifdef WITH_TLS
							// enable TLS Support
							if ( ibrcommon::TLSStream::isInitialized() )
							{
								obj->enableTLS();
							}
#endif

							// add the connection to the connection list
							connectionUp(obj);

							// initialize the object
							obj->initialize();
						} catch (const std::bad_cast&) {

						}
					}

					// breakpoint
					ibrcommon::Thread::yield();
				}
			} catch (const std::exception&) {
				// ignore all errors
				return;
			}
		}

		void TCPConvergenceLayer::__cancellation() throw ()
		{
			_vsocket.down();
		}

		void TCPConvergenceLayer::closeAll()
		{
			// search for an existing connection
			ibrcommon::MutexLock l(_connections_cond);
			for (std::list<TCPConnection*>::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				TCPConnection &conn = *(*iter);

				// close the connection immediately
				conn.shutdown();
			}
		}

		void TCPConvergenceLayer::componentUp() throw ()
		{
			// listen on P2P dial-up events
			dtn::core::EventDispatcher<dtn::net::P2PDialupEvent>::add(this);

			// routine checked for throw() on 15.02.2013
			try {
				// listen on the socket
				_vsocket.up();
				_vsocket_state = true;
			} catch (const ibrcommon::socket_exception &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConvergenceLayer::TAG, error) << "bind failed (" << ex.what() << ")" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void TCPConvergenceLayer::componentDown() throw ()
		{
			// un-listen on P2P dial-up events
			dtn::core::EventDispatcher<dtn::net::P2PDialupEvent>::remove(this);

			// routine checked for throw() on 15.02.2013
			try {
				// shutdown all sockets
				_vsocket.down();
				_vsocket_state = false;
			} catch (const ibrcommon::socket_exception &ex) {
				IBRCOMMON_LOGGER_TAG(TCPConvergenceLayer::TAG, error) << "shutdown failed (" << ex.what() << ")" << IBRCOMMON_LOGGER_ENDL;
			}

			// close all active connections
			closeAll();

			// wait until all tcp connections are down
			{
				ibrcommon::MutexLock l(_connections_cond);
				while (_connections.size() > 0) _connections_cond.wait();
			}
		}
	}

	void TCPConvergenceLayer::getStats(ConvergenceLayer::stats_data &data) const
	{
		std::stringstream ss_format;

		static const std::string IN_TAG = dtn::core::Node::toString(getDiscoveryProtocol()) + "|in";
		static const std::string OUT_TAG = dtn::core::Node::toString(getDiscoveryProtocol()) + "|out";

		ss_format << _stats_in;
		data[IN_TAG] = ss_format.str();
		ss_format.str("");

		ss_format << _stats_out;
		data[OUT_TAG] = ss_format.str();
	}

	void TCPConvergenceLayer::resetStats()
	{
		_stats_in = 0;
		_stats_out = 0;
	}
}
