/*
 * IPNDAgent.cpp
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

#include "net/IPNDAgent.h"
#include "net/DiscoveryAgent.h"
#include "net/P2PDialupEvent.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

#include <ibrdtn/data/Exceptions.h>

#include <ibrcommon/Logger.h>
#include <ibrcommon/TimeMeasurement.h>
#include <ibrcommon/net/socket.h>
#include <ibrcommon/net/vaddress.h>
#include <ibrcommon/link/LinkManager.h>

#ifdef __WIN32__
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#define EADDRINUSE WSAEADDRINUSE
#endif

#include <sstream>
#include <string.h>
#include <typeinfo>
#include <time.h>

namespace dtn
{
	namespace net
	{
		const std::string IPNDAgent::TAG = "IPNDAgent";

		IPNDAgent::IPNDAgent(int port)
		 :
#ifndef __WIN32__
		   _virtual_mcast_iface("__virtual_multicast_interface__"),
#endif
		   _state(false), _port(port)
		{
		}

		IPNDAgent::~IPNDAgent()
		{
			_socket.destroy();
		}

		void IPNDAgent::add(const ibrcommon::vaddress &address) {
			IBRCOMMON_LOGGER_TAG("DiscoveryAgent", info) << "listen to " << address.toString() << IBRCOMMON_LOGGER_ENDL;
			_destinations.insert(address);
		}

		void IPNDAgent::bind(const ibrcommon::vinterface &net)
		{
			IBRCOMMON_LOGGER_TAG("DiscoveryAgent", info) << "add interface " << net.toString() << IBRCOMMON_LOGGER_ENDL;

			// add the interface to the stored set
			ibrcommon::MutexLock l(_interface_lock);

			// only add the interface once
			if (_interfaces.find(net) != _interfaces.end()) return;

			// store the new interface in the list of interfaces
			_interfaces.insert(net);
		}

		void IPNDAgent::join(const ibrcommon::vinterface &iface, const ibrcommon::vaddress &addr) noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "Join on " << iface.toString() << " (" << addr.toString() << ", family: " << addr.family() << ")" << IBRCOMMON_LOGGER_ENDL;

			// only join IPv6 and IPv4 addresses
			if ((addr.family() != AF_INET) && (addr.family() != AF_INET6)) return;

			// do not join on loopback interfaces
			if (addr.isLocal()) return;

			// create a multicast socket and bind to given addr
			ibrcommon::multicastsocket *msock = new ibrcommon::multicastsocket(addr);

			// if we are in UP state
			if (_state) {
				try {
					// bring up
					msock->up();

					// listen to multicast addresses
					for (std::set<ibrcommon::vaddress>::const_iterator it_addr = _destinations.begin(); it_addr != _destinations.end(); ++it_addr)
					{
						try {
							msock->join(*it_addr, iface);
						} catch (const ibrcommon::socket_raw_error &e) {
							if (e.error() == EADDRINUSE) {
								// silent error
							} else if (e.error() == 92) {
								// silent error - protocol not available
							} else {
								IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, warning) << "Join to " << (*it_addr).toString() << " failed on " << iface.toString() << "; " << e.what() << IBRCOMMON_LOGGER_ENDL;
							}
						} catch (const ibrcommon::socket_exception &e) {
							IBRCOMMON_LOGGER_DEBUG_TAG(IPNDAgent::TAG, 10) << "Join to " << (*it_addr).toString() << " failed on " << iface.toString() << "; " << e.what() << IBRCOMMON_LOGGER_ENDL;
						}
					}
				} catch (const ibrcommon::socket_exception &ex) {
					IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, error) << "Join failed on " << iface.toString() << " (" << addr.toString() << ", family: " << addr.family() << ")" << "; " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					delete msock;
					return;
				}
			}

			// add multicast socket to _socket
			_socket.add(msock, iface);
		}

		void IPNDAgent::leave(const ibrcommon::vinterface &iface, const ibrcommon::vaddress &addr) noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "Leave " << iface.toString() << " (" << addr.toString() << ", family: " << addr.family() << ")" << IBRCOMMON_LOGGER_ENDL;

			// get all sockets bound to the given interface
			ibrcommon::socketset ifsocks = _socket.get(iface);

			for (ibrcommon::socketset::iterator it = ifsocks.begin(); it != ifsocks.end(); ++it)
			{
				ibrcommon::multicastsocket *msock = dynamic_cast<ibrcommon::multicastsocket*>(*it);
				if (msock == NULL) continue;

				if (msock->get_address() == addr) {
					// remove the socket
					_socket.remove(msock);

					// shutdown the socket
					try {
						msock->down();
					} catch (const ibrcommon::socket_exception &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(IPNDAgent::TAG, 10) << "leave failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}

					// delete the socket
					delete msock;

					return;
				}
			}
		}

		void IPNDAgent::leave(const ibrcommon::vinterface &iface) noexcept
		{
			// get all sockets bound to the given interface
			ibrcommon::socketset ifsocks = _socket.get(iface);

			for (ibrcommon::socketset::iterator it = ifsocks.begin(); it != ifsocks.end(); ++it)
			{
				ibrcommon::multicastsocket *msock = dynamic_cast<ibrcommon::multicastsocket*>(*it);
				if (msock == NULL) continue;

				// remove the socket
				_socket.remove(msock);

				// shutdown the socket
				try {
					msock->down();
				} catch (const ibrcommon::socket_exception &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG(IPNDAgent::TAG, 10) << "leave failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}

				// delete the socket
				delete msock;
			}
		}

		void IPNDAgent::join(const ibrcommon::vinterface &iface) noexcept
		{
			std::list<ibrcommon::vaddress> addrs = iface.getAddresses();

			for (std::list<ibrcommon::vaddress>::const_iterator it = addrs.begin(); it != addrs.end(); ++it)
			{
				ibrcommon::vaddress addr = (*it);
				addr.setService(_port);
				join(iface, addr);
			}
		}

		void IPNDAgent::onAdvertiseBeacon(const ibrcommon::vinterface &iface, const DiscoveryBeacon &beacon) noexcept
		{
			// serialize announcement
			stringstream ss; ss << beacon;
			const std::string data = ss.str();

			// get all sockets for the given interface
			ibrcommon::socketset fds = _socket.get(iface);

			// send out discovery announcements on all bound sockets
			// (hopefully only one per interface)
			for (ibrcommon::socketset::const_iterator iter = fds.begin(); iter != fds.end(); ++iter)
			{
				try {
					ibrcommon::udpsocket &sock = dynamic_cast<ibrcommon::udpsocket&>(**iter);

					// send beacon to all addresses
					for (std::set<ibrcommon::vaddress>::const_iterator addr_it = _destinations.begin(); addr_it != _destinations.end(); ++addr_it)
					{
						const ibrcommon::vaddress &addr = (*addr_it);

						try {
							// prevent broadcasting in the wrong address family
							if (addr.family() != sock.get_family()) continue;

							sock.sendto(data.c_str(), data.length(), 0, addr);
						} catch (const ibrcommon::socket_exception &e) {
							IBRCOMMON_LOGGER_DEBUG_TAG(IPNDAgent::TAG, 5) << "can not send message to " << addr.toString() << " via " << sock.get_address().toString() << "/" << iface.toString() << "; socket exception: " << e.what() << IBRCOMMON_LOGGER_ENDL;
						} catch (const ibrcommon::vaddress::address_exception &ex) {
							IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
						}
					}
				} catch (const std::bad_cast&) {
					IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, error) << "Socket for sending isn't a udpsocket." << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}

		void IPNDAgent::raiseEvent(const dtn::net::P2PDialupEvent &dialup) noexcept
		{
			switch (dialup.type)
			{
				case dtn::net::P2PDialupEvent::INTERFACE_UP:
				{
					// add the interface to the stored set
					{
						ibrcommon::MutexLock l(_interface_lock);

						// only add the interface once
						if (_interfaces.find(dialup.iface) != _interfaces.end()) break;

						// store the new interface in the list of interfaces
						_interfaces.insert(dialup.iface);
					}

					// subscribe to NetLink events on our interfaces
					ibrcommon::LinkManager::getInstance().addEventListener(dialup.iface, this);

					// register as discovery handler for this interface
					dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(dialup.iface, this);

					// join to all multicast addresses on this interface
					join(dialup.iface);
					break;
				}

				case dtn::net::P2PDialupEvent::INTERFACE_DOWN:
				{
					// check if the interface is bound by us
					{
						ibrcommon::MutexLock l(_interface_lock);

						// only remove the interface if it exists
						if (_interfaces.find(dialup.iface) == _interfaces.end()) break;

						// remove the interface from the stored set
						_interfaces.erase(dialup.iface);
					}

					// subscribe to NetLink events on our interfaces
					ibrcommon::LinkManager::getInstance().removeEventListener(dialup.iface, this);

					// un-register as discovery handler for this interface
					dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(dialup.iface, this);

					// leave the multicast groups on the interface
					leave(dialup.iface);
					break;
				}
			}
		}

		void IPNDAgent::eventNotify(const ibrcommon::LinkEvent &evt)
		{
			// check first if we are really bound to the interface
			{
				ibrcommon::MutexLock l(_interface_lock);
				if (_interfaces.find(evt.getInterface()) == _interfaces.end()) return;
			}

			switch (evt.getAction())
			{
				case ibrcommon::LinkEvent::ACTION_ADDRESS_ADDED:
				{
					ibrcommon::vaddress addr = evt.getAddress();
					addr.setService(_port);
					join(evt.getInterface(), addr);
					break;
				}

				case ibrcommon::LinkEvent::ACTION_ADDRESS_REMOVED:
				{
					ibrcommon::vaddress addr = evt.getAddress();
					addr.setService(_port);
					leave(evt.getInterface(), addr);
					break;
				}

				case ibrcommon::LinkEvent::ACTION_LINK_DOWN:
				{
					// leave the multicast groups on the interface
					leave(evt.getInterface());
					break;
				}

				default:
					break;
			}
		}

		void IPNDAgent::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013

			try {
				// setup the sockets
				_socket.up();

				// set state to UP
				_state = true;
			} catch (const ibrcommon::socket_exception &ex) {
				IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}

#ifndef __WIN32__
			std::set<sa_family_t> bound_set;

			// create a socket for each multicast address and bind explicit
			// to the multicast addresses
			for (std::set<ibrcommon::vaddress>::const_iterator it_addr = _destinations.begin(); it_addr != _destinations.end(); ++it_addr)
			{
				const ibrcommon::vaddress &addr = (*it_addr);

				try {
					sa_family_t fam = addr.family();

					if (bound_set.find(fam) == bound_set.end()) {
						const ibrcommon::vaddress any_addr(_port, fam);

						// create a multicast socket and bind to given addr
						ibrcommon::multicastsocket *msock = new ibrcommon::multicastsocket(any_addr);

						try {
							// bring up
							msock->up();

							// add mcast socket to vsocket
							_socket.add(msock, _virtual_mcast_iface);
						} catch (const ibrcommon::socket_exception &ex) {
							IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, error) << "failed to set-up multicast socket on " << any_addr.toString() << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							delete msock;
						}

						bound_set.insert(fam);
					}
				} catch (const ibrcommon::vaddress::address_exception &ex) {
					IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, error) << "unsupported multicast address " << addr.toString() << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}
#endif

			// listen to P2P dial-up events
			dtn::core::EventDispatcher<dtn::net::P2PDialupEvent>::add(this);

			// join multicast groups and register as discovery handler
			ibrcommon::MutexLock l(_interface_lock);

			for (std::set<ibrcommon::vinterface>::const_iterator it_iface = _interfaces.begin(); it_iface != _interfaces.end(); ++it_iface)
			{
				const ibrcommon::vinterface &iface = (*it_iface);

				// subscribe to NetLink events on our interfaces
				ibrcommon::LinkManager::getInstance().addEventListener(iface, this);

				// register as discovery handler for this interface
				dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(iface, this);

				// join to all multicast addresses on this interface
				join(iface);
			}
		}

		void IPNDAgent::componentDown() noexcept
		{
			// un-listen to P2P dial-up events
			dtn::core::EventDispatcher<dtn::net::P2PDialupEvent>::remove(this);

			// unsubscribe to NetLink events
			ibrcommon::LinkManager::getInstance().removeEventListener(this);

			// un-register as discovery handler for this interface
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(this);

			// mark the send socket as down
			_state = false;

			// shutdown and destroy all sockets
			_socket.destroy();

			ibrcommon::JoinableThread::stop();
			ibrcommon::JoinableThread::join();
		}

		void IPNDAgent::componentRun() noexcept
		{
			struct timeval tv;

			// every second we want to transmit a discovery message, timeout of 1 seconds
			tv.tv_sec = 1;
			tv.tv_usec = 0;

			// get the reference to the discovery agent
			dtn::net::DiscoveryAgent &agent = dtn::core::BundleCore::getInstance().getDiscoveryAgent();

			try {
				while (true)
				{
					ibrcommon::socketset fds;

					try {
						// select on all bound sockets
						_socket.select(&fds, NULL, NULL, &tv);

						// receive from all sockets
						for (ibrcommon::socketset::const_iterator iter = fds.begin(); iter != fds.end(); ++iter)
						{
							ibrcommon::multicastsocket &sock = dynamic_cast<ibrcommon::multicastsocket&>(**iter);

							char data[1500];
							ibrcommon::vaddress sender;
							DiscoveryBeacon beacon = agent.obtainBeacon();

							ssize_t len = sock.recvfrom(data, 1500, 0, sender);

							if (len < 0) return;

							stringstream ss;
							ss.write(data, len);

							try {
								ss >> beacon;

								if (beacon.isShort())
								{
									// generate name with the sender address
									beacon.setEID( dtn::data::EID("udp://[" + sender.address() + "]:4556") );

									// add generated tcpcl service if the services list is empty
									beacon.addService(dtn::net::DiscoveryService(dtn::core::Node::CONN_TCPIP, "ip=" + sender.address() + ";port=4556;"));
								}

								DiscoveryBeacon::service_list &services = beacon.getServices();

								// add source address if not set
								for (dtn::net::DiscoveryBeacon::service_list::iterator iter = services.begin(); iter != services.end(); ++iter) {
									DiscoveryService &service = (*iter);

									if ( (service.getParameters().find("port=") != std::string::npos) &&
											(service.getParameters().find("ip=") == std::string::npos) ) {

										// update service entry
										service.update("ip=" + sender.address() + ";" + service.getParameters());
									}
								}

								// announce the received beacon
								agent.onBeaconReceived(beacon);
							} catch (const dtn::InvalidDataException&) {
							} catch (const ibrcommon::IOException&) {
							}
						}

						// trigger an artificial timeout if the remaining timeout value is zero or below
						if ( tv.tv_sec <= 0 && tv.tv_usec <= 0 )
							throw ibrcommon::vsocket_timeout("timeout");

					} catch (const ibrcommon::vsocket_timeout&) {
						// reset timeout to 1 second
						tv.tv_sec = 1;
						tv.tv_usec = 0;
					}

					yield();
				}
			} catch (const ibrcommon::vsocket_interrupt&) {
				// vsocket has been interrupted - exit now
			} catch (const ibrcommon::socket_exception &ex) {
				// unexpected error - exit now
				IBRCOMMON_LOGGER_TAG(IPNDAgent::TAG, error) << "unexpected error: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void IPNDAgent::__cancellation() noexcept
		{
			// shutdown and interrupt the receiving thread
			_socket.down();
		}

		const std::string IPNDAgent::getName() const
		{
			return "IPNDAgent";
		}
	}
}
