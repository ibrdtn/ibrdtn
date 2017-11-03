/*
 * UDPConvergenceLayer.cpp
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

#include "net/UDPConvergenceLayer.h"
#include "net/TransferAbortedEvent.h"
#include "core/BundleEvent.h"
#include "core/BundleCore.h"
#include "Configuration.h"

#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/data/Serializer.h>

#include <ibrcommon/net/socket.h>
#include <ibrcommon/net/vaddress.h>
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>

#include <iostream>
#include <list>
#include <vector>


using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		const int UDPConvergenceLayer::DEFAULT_PORT = 4556;

		UDPConvergenceLayer::UDPConvergenceLayer(ibrcommon::vinterface net, int port, dtn::data::Length mtu)
		 : _net(net), _port(port), m_maxmsgsize(mtu), _running(false), _stats_in(0), _stats_out(0)
		{
		}

		UDPConvergenceLayer::~UDPConvergenceLayer()
		{
			componentDown();
		}

		void UDPConvergenceLayer::resetStats()
		{
			_stats_in = 0;
			_stats_out = 0;
		}

		void UDPConvergenceLayer::getStats(ConvergenceLayer::stats_data &data) const
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

		dtn::core::Node::Protocol UDPConvergenceLayer::getDiscoveryProtocol() const
		{
			return dtn::core::Node::CONN_UDPIP;
		}

		void UDPConvergenceLayer::onUpdateBeacon(const ibrcommon::vinterface &iface, DiscoveryBeacon &announcement) throw (dtn::net::DiscoveryBeaconHandler::NoServiceHereException)
		{
			// announce port only if we are bound to any interface
			if (_net.isAny()) {
				std::stringstream service;
				// ... set the port only
				service << "port=" << _port << ";";
				announcement.addService( DiscoveryService(getDiscoveryProtocol(), service.str()));
				return;
			}

			// do not announce if this is not our interface
			if (iface != _net) throw dtn::net::DiscoveryBeaconHandler::NoServiceHereException();
			
			// determine if we should enable crosslayer discovery by sending out our own address
			bool crosslayer = dtn::daemon::Configuration::getInstance().getDiscovery().enableCrosslayer();

			// this marker should set to true if we added an service description
			bool announced = false;

			try {
				// check if cross layer discovery is disabled
				if (!crosslayer) throw ibrcommon::Exception("crosslayer discovery disabled!");

				// get all global addresses of the interface
				std::list<ibrcommon::vaddress> list = _net.getAddresses();

				// if no address is returned... (goto catch block)
				if (list.empty()) throw ibrcommon::Exception("no address found");

				for (std::list<ibrcommon::vaddress>::const_iterator addr_it = list.begin(); addr_it != list.end(); ++addr_it)
				{
					const ibrcommon::vaddress &addr = (*addr_it);

					// only announce global scope addresses
					if (addr.scope() != ibrcommon::vaddress::SCOPE_GLOBAL) continue;

					try {
						// do not announce non-IP addresses
						sa_family_t f = addr.family();
						if ((f != AF_INET) && (f != AF_INET6)) continue;

						std::stringstream service;
						// fill in the ip address
						service << "ip=" << addr.address() << ";port=" << _port << ";";
						announcement.addService( DiscoveryService(getDiscoveryProtocol(), service.str()));

						// set the announce mark
						announced = true;
					} catch (const ibrcommon::vaddress::address_exception &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG("UDPConvergenceLayer", 25) << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}
			} catch (const ibrcommon::Exception&) {
				// address collection process aborted
			}

			// if we still not announced anything...
			if (!announced) {
				// announce at least our local port
				std::stringstream service;
				service << "port=" << _port << ";";
				announcement.addService( DiscoveryService(getDiscoveryProtocol(), service.str()));
			}
		}

		void UDPConvergenceLayer::queue(const dtn::core::Node &node, const dtn::net::BundleTransfer &job)
		{
			const std::list<dtn::core::Node::URI> uri_list = node.get(dtn::core::Node::CONN_UDPIP);
			if (uri_list.empty())
			{
				dtn::net::BundleTransfer local_job = job;
				local_job.abort(dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
				return;
			}

			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

			const dtn::core::Node::URI &uri = uri_list.front();

			std::string address = "0.0.0.0";
			unsigned int port = 0;

			// read values
			uri.decode(address, port);

			// get the address of the node
			ibrcommon::vaddress addr(address, port);

			try {
				// read the bundle out of the storage
				dtn::data::Bundle bundle = storage.get(job.getBundle());

				// create a filter context
				dtn::core::FilterContext context;
				context.setPeer(node.getEID());
				context.setProtocol(getDiscoveryProtocol());

				// push bundle through the filter routines
				context.setBundle(bundle);
				BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, bundle);

				switch (ret) {
					case BundleFilter::ACCEPT:
						break;
					case BundleFilter::REJECT:
					case BundleFilter::DROP:
						dtn::net::BundleTransfer local_job = job;
						local_job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER);
						return;
				}

				// build the dictionary for EID lookup
				const dtn::data::Dictionary dict(bundle);

				// create a default serializer
				dtn::data::DefaultSerializer dummy(std::cout, dict);

				// get the encoded length of the primary block
				size_t header = dummy.getLength((const PrimaryBlock&)bundle);
				header += 20; // two times SDNV through fragmentation

				dtn::data::Length size = dummy.getLength(bundle);

				if (size > m_maxmsgsize)
				{
					// abort transmission if fragmentation is disabled
					if (!dtn::daemon::Configuration::getInstance().getNetwork().doFragmentation()
							&& !bundle.get(dtn::data::PrimaryBlock::DONT_FRAGMENT))
					{
						throw ConnectionInterruptedException();
					}

					const size_t psize = bundle.find<dtn::data::PayloadBlock>().getLength();
					const size_t fragment_size = m_maxmsgsize - header;
					const size_t fragment_count = (psize / fragment_size) + (((psize % fragment_size) > 0) ? 1 : 0);

					IBRCOMMON_LOGGER_DEBUG_TAG("UDPConvergenceLayer", 30) << "MTU of " << m_maxmsgsize << " is too small to carry " << psize << " bytes of payload." << IBRCOMMON_LOGGER_ENDL;
					IBRCOMMON_LOGGER_DEBUG_TAG("UDPConvergenceLayer", 30) << "create " << fragment_count << " fragments with " << fragment_size << " bytes each." << IBRCOMMON_LOGGER_ENDL;

					for (size_t i = 0; i < fragment_count; ++i)
					{
						dtn::data::BundleFragment fragment(bundle, i * fragment_size, fragment_size);

						std::stringstream ss;
						dtn::data::DefaultSerializer serializer(ss);

						serializer << fragment;
						std::string data = ss.str();

						// send out the bundle data
						send(addr, data);
					}
				}
				else
				{
					std::stringstream ss;
					dtn::data::DefaultSerializer serializer(ss);

					serializer << bundle;
					std::string data = ss.str();

					// send out the bundle data
					send(addr, data);
				}

				// success - raise bundle event
				dtn::net::BundleTransfer local_job = job;
				local_job.complete();
			} catch (const dtn::storage::NoBundleFoundException&) {
				// send transfer aborted event
				dtn::net::BundleTransfer local_job = job;
				local_job.abort(dtn::net::TransferAbortedEvent::REASON_BUNDLE_DELETED);
			} catch (const ibrcommon::socket_exception&) {
				// CL is busy, requeue bundle
			} catch (const NoAddressFoundException &ex) {
				// no connection available
				dtn::net::BundleTransfer local_job = job;
				local_job.abort(dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
			}
		}

		void UDPConvergenceLayer::send(const ibrcommon::vaddress &addr, const std::string &data) throw (ibrcommon::socket_exception, NoAddressFoundException)
		{
			// set write lock
			ibrcommon::MutexLock l(m_writelock);

			// get the first global scope socket
			ibrcommon::socketset socks = _vsocket.getAll();
			for (ibrcommon::socketset::iterator iter = socks.begin(); iter != socks.end(); ++iter) {
				ibrcommon::udpsocket &sock = dynamic_cast<ibrcommon::udpsocket&>(**iter);

				// send converted line back to client.
				sock.sendto(data.c_str(), data.length(), 0, addr);

				// add statistic data
				_stats_out += data.length();

				// success
				return;
			}

			// failure
			throw NoAddressFoundException("no valid address found");
		}

		void UDPConvergenceLayer::receive(dtn::data::Bundle &bundle, dtn::data::EID &sender) throw (ibrcommon::socket_exception, dtn::InvalidDataException)
		{
			ibrcommon::MutexLock l(m_readlock);

			std::vector<char> data(m_maxmsgsize);

			// data waiting
			ibrcommon::socketset readfds;

			// wait for incoming messages
			_vsocket.select(&readfds, NULL, NULL, NULL);

			if (readfds.size() > 0) {
				ibrcommon::datagramsocket *sock = static_cast<ibrcommon::datagramsocket*>(*readfds.begin());

				ibrcommon::vaddress fromaddr;
				size_t len = sock->recvfrom(&data[0], m_maxmsgsize, 0, fromaddr);

				// add statistic data
				_stats_in += len;

				std::stringstream ss; ss << "udp://" << fromaddr.toString();
				sender = dtn::data::EID(ss.str());

				if (len > 0)
				{
					// read all data into a stream
					std::stringstream ss;
					ss.write(&data[0], len);

					// get the bundle
					dtn::data::DefaultDeserializer(ss, dtn::core::BundleCore::getInstance()) >> bundle;
				}
			}
		}

		void UDPConvergenceLayer::eventNotify(const ibrcommon::LinkEvent &evt)
		{
			if (evt.getInterface() != _net) return;

			switch (evt.getAction())
			{
				case ibrcommon::LinkEvent::ACTION_ADDRESS_ADDED:
				{
					ibrcommon::vaddress bindaddr = evt.getAddress();
					// convert the port into a string
					std::stringstream ss; ss << _port;
					bindaddr.setService(ss.str());
					ibrcommon::udpsocket *sock = new ibrcommon::udpsocket(bindaddr);
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
						ibrcommon::udpsocket *sock = dynamic_cast<ibrcommon::udpsocket*>(*iter);
						if (sock->get_address().address() == evt.getAddress().address()) {
							_vsocket.remove(sock);
							sock->down();
							delete sock;
							break;
						}
					}
					break;
				}

				case ibrcommon::LinkEvent::ACTION_LINK_DOWN:
				{
					ibrcommon::socketset socks = _vsocket.get(evt.getInterface());
					for (ibrcommon::socketset::iterator iter = socks.begin(); iter != socks.end(); ++iter) {
						ibrcommon::udpsocket *sock = dynamic_cast<ibrcommon::udpsocket*>(*iter);
						_vsocket.remove(sock);
						sock->down();
						delete sock;
					}
					break;
				}

				default:
					break;
			}
		}

		void UDPConvergenceLayer::componentUp() throw ()
		{
			// routine checked for throw() on 15.02.2013
			try {
				// create sockets for all addresses on the interface
				std::list<ibrcommon::vaddress> addrs = _net.getAddresses();

				// convert the port into a string
				std::stringstream ss; ss << _port;

				for (std::list<ibrcommon::vaddress>::iterator iter = addrs.begin(); iter != addrs.end(); ++iter) {
					ibrcommon::vaddress &addr = (*iter);

					try {
						// handle the addresses according to their family
						switch (addr.family()) {
						case AF_INET:
						case AF_INET6:
							addr.setService(ss.str());
							_vsocket.add(new ibrcommon::udpsocket(addr), _net);
							break;
						default:
							break;
						}
					} catch (const ibrcommon::vaddress::address_exception &ex) {
						IBRCOMMON_LOGGER_TAG("UDPConvergenceLayer", warning) << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}

				_vsocket.up();

				// subscribe to NetLink events on our interfaces
				ibrcommon::LinkManager::getInstance().addEventListener(_net, this);

				// register as discovery handler for this interface
				dtn::core::BundleCore::getInstance().getDiscoveryAgent().registerService(_net, this);
			} catch (const ibrcommon::socket_exception &ex) {
				IBRCOMMON_LOGGER_TAG("UDPConvergenceLayer", error) << "bind failed (" << ex.what() << ")" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void UDPConvergenceLayer::componentDown() throw ()
		{
			// unsubscribe to NetLink events
			ibrcommon::LinkManager::getInstance().removeEventListener(this);

			// unregister as discovery handler for this interface
			dtn::core::BundleCore::getInstance().getDiscoveryAgent().unregisterService(_net, this);

			_vsocket.destroy();
			stop();
			join();
		}

		void UDPConvergenceLayer::componentRun() throw ()
		{
			_running = true;

			// create a filter context
			dtn::core::FilterContext context;
			context.setProtocol(getDiscoveryProtocol());

			while (_running)
			{
				try {
					dtn::data::Bundle bundle;
					EID sender;

					receive(bundle, sender);

					// push bundle through the filter routines
					context.setBundle(bundle);
					BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, bundle);

					switch (ret) {
						case BundleFilter::ACCEPT:
							// inject bundle into core
							dtn::core::BundleCore::getInstance().inject(sender, bundle, false);
							break;

						case BundleFilter::REJECT:
						case BundleFilter::DROP:
							break;
					}

				} catch (const dtn::InvalidDataException &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG("UDPConvergenceLayer", 2) << "Received a invalid bundle: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				} catch (const std::exception&) {
					return;
				}
				yield();
			}
		}

		void UDPConvergenceLayer::__cancellation() throw ()
		{
			_running = false;
			_vsocket.down();
		}

		const std::string UDPConvergenceLayer::getName() const
		{
			return "UDPConvergenceLayer";
		}
	}
}
