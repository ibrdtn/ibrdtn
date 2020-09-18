/*
 * ApiServer.cpp
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

#include "config.h"
#include "Configuration.h"
#include "api/ApiServer.h"
#include "core/EventDispatcher.h"
#include "core/BundleCore.h"

#include <ibrcommon/net/vaddress.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/net/vsocket.h>
#include <ibrcommon/net/socketstream.h>

#include <typeinfo>
#include <algorithm>
#include <sstream>
#include <unistd.h>
#include <list>

namespace dtn
{
	namespace api
	{
		const std::string ApiServer::TAG = "ApiServer";

		ApiServer::ApiServer(const ibrcommon::File &socketfile)
		 : _shutdown(false), _garbage_collector(*this)
		{
			_sockets.add(new ibrcommon::fileserversocket(socketfile));
		}

		ApiServer::ApiServer(const ibrcommon::vinterface &net, int port)
		 : _shutdown(false), _garbage_collector(*this)
		{
			if (net.isLoopback()) {
				if (ibrcommon::basesocket::hasSupport(AF_INET6)) {
					ibrcommon::vaddress addr6(ibrcommon::vaddress::VADDR_LOCALHOST, port, AF_INET6);
					_sockets.add(new ibrcommon::tcpserversocket(addr6, 5));
				}

				ibrcommon::vaddress addr4(ibrcommon::vaddress::VADDR_LOCALHOST, port, AF_INET);
				_sockets.add(new ibrcommon::tcpserversocket(addr4, 5));
			}
			else if (net.isAny()) {
				ibrcommon::vaddress addr(ibrcommon::vaddress::VADDR_ANY, port);
				_sockets.add(new ibrcommon::tcpserversocket(addr, 5));
			}
			else {
				// add a socket for each address on the interface
				std::list<ibrcommon::vaddress> addrs = net.getAddresses();

				// convert the port into a string
				std::stringstream ss; ss << port;

				for (std::list<ibrcommon::vaddress>::iterator iter = addrs.begin(); iter != addrs.end(); ++iter) {
					ibrcommon::vaddress &addr = (*iter);

					try {
						// handle the addresses according to their family
						switch (addr.family()) {
						case AF_INET:
						case AF_INET6:
							addr.setService(ss.str());
							_sockets.add(new ibrcommon::tcpserversocket(addr, 5), net);
							break;

						default:
							break;
						}
					} catch (const ibrcommon::vaddress::address_exception &ex) {
						IBRCOMMON_LOGGER_TAG(ApiServer::TAG, warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}
			}
		}

		ApiServer::~ApiServer()
		{
			_garbage_collector.stop();
			_garbage_collector.join();
			join();

			_sockets.destroy();
		}

		void ApiServer::__cancellation() noexcept
		{
			// shut-down all server sockets
			_sockets.down();
		}

		void ApiServer::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013
			try {
				// bring up all server sockets
				_sockets.up();
			} catch (const ibrcommon::socket_exception &ex) {
				IBRCOMMON_LOGGER_TAG(ApiServer::TAG, error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
			
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(this);
			startGarbageCollector();
		}

		void ApiServer::componentRun() noexcept
		{
			try {
				while (!_shutdown)
				{
					// create a socket set to do select on all sockets
					ibrcommon::socketset fds;

					// do select on all socket and find all readable ones
					_sockets.select(&fds, NULL, NULL, NULL);

					// iterate through all readable sockets
					for (ibrcommon::socketset::iterator iter = fds.begin(); iter != fds.end(); ++iter)
					{
						// we assume all the sockets in _sockets are server sockets
						// so cast this one to the right class
						ibrcommon::serversocket &sock = dynamic_cast<ibrcommon::serversocket&>(**iter);

						// variable to store the peer address on the next accept call
						ibrcommon::vaddress peeraddr;
						
						// accept the next client
						ibrcommon::clientsocket *peersock = sock.accept(peeraddr);

						// set the no delay option for the new socket if configured
						if ( dtn::daemon::Configuration::getInstance().getNetwork().getTCPOptionNoDelay() )
						{
							// check if the socket is a tcpsocket
							if ( dynamic_cast<ibrcommon::tcpsocket*>(peersock) != NULL )
							{
								peersock->set(ibrcommon::clientsocket::NO_DELAY, true);
							}
						}

						// create a new socket stream using the new client socket
						// now the socket stream is responsible for the client socket
						// and will destroy the instance on its own destruction
						ibrcommon::socketstream *conn = new ibrcommon::socketstream(peersock);

						// if we are already in shutdown state
						if (_shutdown)
						{
							// close the new socket
							conn->close();
							
							// and free the object
							delete conn;
							return;
						}

						// generate some output
						IBRCOMMON_LOGGER_DEBUG_TAG("ApiServer", 5) << "new connected client at the extended API server" << IBRCOMMON_LOGGER_ENDL;

						// send welcome banner
						(*conn) << "IBR-DTN " << dtn::daemon::Configuration::getInstance().version() << " API 1.0" << std::endl;

						// the new client object will be hold here
						ClientHandler *obj = NULL;
						
						// in locked state we create a new registration for the new connection
						{
							ibrcommon::MutexLock l1(_registration_lock);
							
							// create a new registration
							Registration *reg = new Registration();
							_registrations.push_back(reg);
							IBRCOMMON_LOGGER_DEBUG_TAG("ApiServer", 5) << "new registration " << reg->getHandle() << IBRCOMMON_LOGGER_ENDL;

							// create a new clienthandler for the new registration
							obj  = new ClientHandler(*this, *_registrations.back(), conn);
						}

						// one again in locked state we push the new connection in the connection list
						{
							ibrcommon::MutexLock l2(_connection_lock);
							_connections.push_back(obj);
						}

						// start the client handler
						obj->start();
					}

					// breakpoint
					ibrcommon::Thread::yield();
				}
			} catch (const std::exception&) {
				// ignore all errors
				return;
			}
		}

		void ApiServer::componentDown() noexcept
		{
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(this);

			// put the server into shutdown mode
			_shutdown = true;
			
			// close the listen API socket
			_sockets.down();

			// pause the garbage collection
			_garbage_collector.pause();

			// stop/close all connections in locked state
			{
				ibrcommon::MutexLock l(_connection_lock);

				// shutdown all clients
				for (client_list::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
				{
					(*iter)->stop();
				}
			}

			// wait until all clients are down
			while (_connections.size() > 0) ibrcommon::Thread::sleep(1000);
		}

		Registration& ApiServer::getRegistration(const std::string &handle)
		{
			ibrcommon::MutexLock l(_registration_lock);
			for (registration_list::iterator iter = _registrations.begin(); iter != _registrations.end(); ++iter)
			{
				Registration &reg = (**iter);

				if (reg == handle)
				{
					if (reg.isPersistent()){
						reg.attach();
						return reg;
					}
					break;
				}
			}

			throw Registration::NotFoundException("Registration not found");
		}

		size_t ApiServer::timeout(ibrcommon::Timer*)
		{
			{
				ibrcommon::MutexLock l(_registration_lock);

				// remove non-persistent and detached registrations
				for (registration_list::iterator iter = _registrations.begin(); iter != _registrations.end();)
				{
					try
					{
						Registration *reg = (*iter);

						reg->attach();
						if(!reg->isPersistent()){
							IBRCOMMON_LOGGER_DEBUG_TAG("ApiServer", 5) << "release registration " << reg->getHandle() << IBRCOMMON_LOGGER_ENDL;
							_registrations.erase(iter++);
							delete reg;
						}
						else
						{
							reg->detach();
							++iter;
						}
					}
					catch(const Registration::AlreadyAttachedException &ex)
					{
						++iter;
					}
				}
			}

			return nextRegistrationExpiry();
		}

		const std::string ApiServer::getName() const
		{
			return "ApiServer";
		}

		void ApiServer::connectionUp(ClientHandler*)
		{
			// generate some output
			IBRCOMMON_LOGGER_DEBUG_TAG("ApiServer", 5) << "api connection up" << IBRCOMMON_LOGGER_ENDL;
		}

		void ApiServer::connectionDown(ClientHandler *obj)
		{
			// generate some output
			IBRCOMMON_LOGGER_DEBUG_TAG("ApiServer", 5) << "api connection down" << IBRCOMMON_LOGGER_ENDL;

			ibrcommon::MutexLock l(_connection_lock);

			// remove this object out of the list
			for (client_list::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				if (obj == (*iter))
				{
					_connections.erase(iter);
					break;
				}
			}
		}

		void ApiServer::freeRegistration(Registration &reg)
		{
			if(reg.isPersistent())
			{
				reg.detach();
				startGarbageCollector();
			}
			else
			{
				ibrcommon::MutexLock l(_registration_lock);
				// remove the registration
				for (registration_list::iterator iter = _registrations.begin(); iter != _registrations.end(); ++iter)
				{
					Registration *r = (*iter);

					if (reg == (*r))
					{
						IBRCOMMON_LOGGER_DEBUG_TAG("ApiServer", 5) << "release registration " << reg.getHandle() << IBRCOMMON_LOGGER_ENDL;
						_registrations.erase(iter);
						delete r;
						break;
					}
				}
			}
		}

		void ApiServer::raiseEvent(const dtn::routing::QueueBundleEvent &queued) noexcept
		{
			// ignore fragments - we can not deliver them directly to the client
			if (queued.bundle.isFragment()) return;

			ibrcommon::MutexLock l(_connection_lock);
			for (client_list::iterator iter = _connections.begin(); iter != _connections.end(); ++iter)
			{
				ClientHandler &conn = **iter;
				if (conn.getRegistration().hasSubscribed(queued.bundle.destination))
				{
					conn.getRegistration().notify(Registration::NOTIFY_BUNDLE_AVAILABLE);
				}
			}
		}

		void ApiServer::startGarbageCollector()
		{
			try
			{
				/* set the timeout for the GarbageCollector */
				size_t timeout = nextRegistrationExpiry();
				_garbage_collector.set(timeout);

				/* start it, if it is not running yet */
				if(!_garbage_collector.isRunning())
					_garbage_collector.start();
			}
			catch(const ibrcommon::Timer::StopTimerException &ex)
			{
			}
		}

		size_t ApiServer::nextRegistrationExpiry()
		{
			ibrcommon::MutexLock l(_registration_lock);
			bool persistentFound = false;
			size_t new_timeout = 0;
			size_t current_time = ibrcommon::Timer::get_current_time();

			// find the registration that expires next
			for (registration_list::iterator iter = _registrations.begin(); iter != _registrations.end(); ++iter)
			{
				try
				{
					Registration &reg = (**iter);

					reg.attach();
					if(!reg.isPersistent()){
						/* found an expired registration, trigger the timer */
						reg.detach();
						new_timeout = 0;
						persistentFound = true;
						break;
					}
					else
					{
						size_t expire_time = reg.getExpireTime();
						/* we dont have to check if the expire time is smaller then the current_time
							since isPersistent() would have returned false */
						size_t expire_timeout = expire_time - current_time;
						reg.detach();

						/* if persistentFound is false, no persistent registration was found yet */
						if(!persistentFound)
						{
							persistentFound = true;
							new_timeout = expire_timeout;
						}
						else if(expire_timeout < new_timeout)
						{
							new_timeout = expire_timeout;
						}
					}
				}
				catch(const Registration::AlreadyAttachedException &ex)
				{
				}
			}

			if(!persistentFound) throw ibrcommon::Timer::StopTimerException();

			return new_timeout;
		}
	}
}
