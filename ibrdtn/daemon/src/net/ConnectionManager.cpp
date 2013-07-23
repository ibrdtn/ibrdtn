/*
 * ConnectionManager.cpp
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
#include "net/ConnectionManager.h"
#include "net/UDPConvergenceLayer.h"
#include "net/TCPConvergenceLayer.h"
#include "net/ConnectionEvent.h"
#include "core/EventDispatcher.h"
#include "core/NodeEvent.h"
#include "core/BundleEvent.h"
#include "core/BundleCore.h"
#include "core/NodeEvent.h"
#include "core/TimeEvent.h"
#include "core/GlobalEvent.h"

#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <typeinfo>

using namespace dtn::core;

namespace dtn
{
	namespace net
	{
		struct CompareNodeDestination:
		public std::binary_function< dtn::core::Node, dtn::data::EID, bool > {
			bool operator() ( const dtn::core::Node &node, const dtn::data::EID &destination ) const {
				return node.getEID() == destination;
			}
		};

		ConnectionManager::ConnectionManager()
		 : _next_autoconnect(0)
		{
		}

		ConnectionManager::~ConnectionManager()
		{
		}

		void ConnectionManager::componentUp() throw ()
		{
			// routine checked for throw() on 15.02.2013
			dtn::core::EventDispatcher<TimeEvent>::add(this);
			dtn::core::EventDispatcher<NodeEvent>::add(this);
			dtn::core::EventDispatcher<ConnectionEvent>::add(this);
			dtn::core::EventDispatcher<GlobalEvent>::add(this);

			// set next auto connect
			const dtn::daemon::Configuration::Network &nc = dtn::daemon::Configuration::getInstance().getNetwork();
			if (nc.getAutoConnect() != 0)
			{
				_next_autoconnect = dtn::utils::Clock::getTime() + nc.getAutoConnect();
			}
		}

		void ConnectionManager::componentDown() throw ()
		{
			{
				ibrcommon::MutexLock l(_cl_lock);
				// clear the list of convergence layers
				_cl.clear();
			}

			{
				ibrcommon::MutexLock l(_node_lock);
				// clear the node list
				_nodes.clear();
			}

			_next_autoconnect = 0;

			dtn::core::EventDispatcher<NodeEvent>::remove(this);
			dtn::core::EventDispatcher<TimeEvent>::remove(this);
			dtn::core::EventDispatcher<ConnectionEvent>::remove(this);
			dtn::core::EventDispatcher<GlobalEvent>::remove(this);
		}

		void ConnectionManager::raiseEvent(const dtn::core::Event *evt) throw ()
		{
			try {
				const NodeEvent &nodeevent = dynamic_cast<const NodeEvent&>(*evt);
				const Node &n = nodeevent.getNode();

				switch (nodeevent.getAction())
				{
					case NODE_AVAILABLE:
						if (n.doConnectImmediately())
						{
							try {
								// open the connection immediately
								open(n);
							} catch (const ibrcommon::Exception&) {
								// ignore errors
							}
						}
						break;

					default:
						break;
				}
				return;
			} catch (const std::bad_cast&) { }

			try {
				const TimeEvent &timeevent = dynamic_cast<const TimeEvent&>(*evt);

				if (timeevent.getAction() == TIME_SECOND_TICK)
				{
					check_unavailable();
					check_autoconnect();
				}
				return;
			} catch (const std::bad_cast&) { }

			try {
				const ConnectionEvent &connection = dynamic_cast<const ConnectionEvent&>(*evt);

				switch (connection.state)
				{
					case ConnectionEvent::CONNECTION_UP:
					{
						add(connection.node);
						break;
					}

					case ConnectionEvent::CONNECTION_DOWN:
					{
						remove(connection.node);
						break;
					}

					default:
						break;
				}
				return;
			} catch (const std::bad_cast&) { }

			try {
				const dtn::core::GlobalEvent &global = dynamic_cast<const dtn::core::GlobalEvent&>(*evt);
				switch (global.getAction()) {
				case GlobalEvent::GLOBAL_INTERNET_AVAILABLE:
					check_available();
					break;

				case GlobalEvent::GLOBAL_INTERNET_UNAVAILABLE:
					check_unavailable();
					break;

				default:
					break;
				}
			} catch (const std::bad_cast&) { };
		}

		void ConnectionManager::add(const dtn::core::Node &n)
		{
			// If node contains MCL
			if(n.has(Node::CONN_EMAIL) && !n.getEID().getScheme().compare("mail") == 0)
			{
				dtn::core::Node node = n;

				std::list<Node::URI> uri = node.get(Node::CONN_EMAIL);
				for(std::list<Node::URI>::iterator iter = uri.begin(); iter != uri.end(); iter++)
				{
					std::string address;
					unsigned int port;
					(*iter).decode(address, port);

					dtn::core::Node fakeNode("mail://" + address);

					fakeNode.add(Node::URI((*iter).type, Node::CONN_EMAIL, "email=" + address + ";", (*iter).expire - dtn::utils::Clock::getTime(), (*iter).priority));

					// Announce faked node
					updateNeighbor(fakeNode);

					// Add route to faked node
					dtn::core::BundleCore::getInstance().addRoute(node.getEID(), fakeNode.getEID(), 0);

					node.remove((*iter));

					if(node.getAll().size() == 0)
						return;
				}

				updateNeighbor(node);

				return;
			}

			ibrcommon::MutexLock l(_node_lock);
			pair<nodemap::iterator,bool> ret = _nodes.insert( pair<dtn::data::EID, dtn::core::Node>(n.getEID(), n) );

			dtn::core::Node &db = (*(ret.first)).second;

			if (!ret.second) {
				dtn::data::Size old = db.size();

				// add all attributes to the node in the database
				db += n;

				if (old != db.size()) {
					// announce the new node
					dtn::core::NodeEvent::raise(db, dtn::core::NODE_DATA_ADDED);
				}
			} else {
				IBRCOMMON_LOGGER_DEBUG_TAG("ConnectionManager", 56) << "New node available: " << db << IBRCOMMON_LOGGER_ENDL;
			}

			if (db.isAvailable() && !db.isAnnounced()) {
				db.setAnnounced(true);

				// announce the new node
				dtn::core::NodeEvent::raise(db, dtn::core::NODE_AVAILABLE);
			}
		}

		void ConnectionManager::remove(const dtn::core::Node &n)
		{
			ibrcommon::MutexLock l(_node_lock);
			try {
				dtn::core::Node &db = getNode(n.getEID());

				dtn::data::Size old = db.size();

				// erase all attributes to the node in the database
				db -= n;

				if (old != db.size()) {
					// announce the new node
					dtn::core::NodeEvent::raise(db, dtn::core::NODE_DATA_REMOVED);
				}

				IBRCOMMON_LOGGER_DEBUG_TAG("ConnectionManager", 56) << "Node attributes removed: " << db << IBRCOMMON_LOGGER_ENDL;
			} catch (const NeighborNotAvailableException&) { };
		}

		void ConnectionManager::add(ConvergenceLayer *cl)
		{
			ibrcommon::MutexLock l(_cl_lock);
			_cl.insert( cl );
		}

		void ConnectionManager::remove(ConvergenceLayer *cl)
		{
			ibrcommon::MutexLock l(_cl_lock);
			_cl.erase( cl );
		}

		ConnectionManager::stats_list ConnectionManager::getStats()
		{
			ibrcommon::MutexLock l(_cl_lock);
			stats_list stats;

			for (std::set<ConvergenceLayer*>::const_iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
			{
				ConvergenceLayer &cl = (**iter);
				const ConvergenceLayer::stats_map &cl_stats = cl.getStats();
				const dtn::core::Node::Protocol p = cl.getDiscoveryProtocol();

				stats_pair entry;
				entry.first = p;
				entry.second = cl_stats;

				stats.push_back(entry);
			}

			return stats;
		}

		void ConnectionManager::resetStats()
		{
			ibrcommon::MutexLock l(_cl_lock);
			for (std::set<ConvergenceLayer*>::iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
			{
				ConvergenceLayer &cl = (**iter);
				cl.resetStats();
			}
		}

		void ConnectionManager::add(P2PDialupExtension *ext)
		{
			ibrcommon::MutexLock l(_dialup_lock);
			_dialups.insert(ext);
		}

		void ConnectionManager::remove(P2PDialupExtension *ext)
		{
			ibrcommon::MutexLock l(_dialup_lock);
			_dialups.erase(ext);
		}

		void ConnectionManager::discovered(const dtn::core::Node &node)
		{
			// ignore messages of ourself
			if (node.getEID() == dtn::core::BundleCore::local) return;

			// add node or its attributes to the database
			add(node);
		}

		void ConnectionManager::check_available()
		{
			ibrcommon::MutexLock l(_node_lock);

			// search for outdated nodes
			for (nodemap::iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
			{
				dtn::core::Node &n = (*iter).second;
				if (n.isAnnounced()) continue;

				if (n.isAvailable()) {
					n.setAnnounced(true);

					// announce the unavailable event
					dtn::core::NodeEvent::raise(n, dtn::core::NODE_AVAILABLE);
				}
			}
		}

		void ConnectionManager::check_unavailable()
		{
			ibrcommon::MutexLock l(_node_lock);

			// search for outdated nodes
			nodemap::iterator iter = _nodes.begin();
			while ( iter != _nodes.end() )
			{
				dtn::core::Node &n = (*iter).second;
				if (!n.isAnnounced()) {
					++iter;
					continue;
				}

				if ( !n.isAvailable() ) {
					n.setAnnounced(false);

					// announce the unavailable event
					dtn::core::NodeEvent::raise(n, dtn::core::NODE_UNAVAILABLE);
				}

				if ( n.expire() )
				{
					if (n.isAnnounced()) {
						// announce the unavailable event
						dtn::core::NodeEvent::raise(n, dtn::core::NODE_UNAVAILABLE);
					}

					// remove the element
					_nodes.erase( iter++ );
				}
				else
				{
					++iter;
				}
			}
		}

		void ConnectionManager::check_autoconnect()
		{
			std::queue<dtn::core::Node> _connect_nodes;

			dtn::data::Timeout interval = dtn::daemon::Configuration::getInstance().getNetwork().getAutoConnect();
			if (interval == 0) return;

			if (_next_autoconnect < dtn::utils::Clock::getTime())
			{
				// search for non-connected but available nodes
				ibrcommon::MutexLock l(_cl_lock);
				for (nodemap::const_iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
				{
					const Node &n = (*iter).second;
					std::list<Node::URI> ul = n.get(Node::NODE_CONNECTED, Node::CONN_TCPIP);

					if (ul.empty() && n.isAvailable())
					{
						_connect_nodes.push(n);
					}
				}

				// set the next check time
				_next_autoconnect = dtn::utils::Clock::getTime() + interval;
			}

			while (!_connect_nodes.empty())
			{
				try {
					open(_connect_nodes.front());
				} catch (const ibrcommon::Exception&) {
					// ignore errors
				}
				_connect_nodes.pop();
			}
		}

		void ConnectionManager::open(const dtn::core::Node &node) throw (ibrcommon::Exception)
		{
			ibrcommon::MutexLock l(_cl_lock);

			// search for the right cl
			for (std::set<ConvergenceLayer*>::iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
			{
				ConvergenceLayer *cl = (*iter);
				if (node.has(cl->getDiscoveryProtocol()))
				{
					cl->open(node);

					// stop here, we queued the bundle already
					return;
				}
			}

			// throw dial-up exception if there are P2P dial-up connections available
			if (node.hasDialup())
			{
				// trigger the dial-up connection
				dialup(node);

				throw dtn::core::P2PDialupException();
			}

			throw dtn::net::ConnectionNotAvailableException();
		}

		void ConnectionManager::dialup(const dtn::core::Node &n)
		{
			// search for p2p_dialup connections
			ibrcommon::MutexLock l(_cl_lock);

			// get the list of all available URIs
			std::list<Node::URI> uri_list = n.get(Node::NODE_P2P_DIALUP);

			// trigger p2p_dialup connections
			for (std::list<Node::URI>::const_iterator it = uri_list.begin(); it != uri_list.end(); ++it)
			{
				const dtn::core::Node::URI &uri = (*it);

				ibrcommon::MutexLock l(_dialup_lock);
				for (std::set<P2PDialupExtension*>::iterator iter = _dialups.begin(); iter != _dialups.end(); ++iter)
				{
					P2PDialupExtension &p2pext = (**iter);

					if (uri.protocol == p2pext.getProtocol()) {
						// trigger connection set-up
						p2pext.connect(uri);
					}
				}
			}
		}

		void ConnectionManager::queue(const dtn::core::Node &node, const dtn::net::BundleTransfer &job)
		{
			ibrcommon::MutexLock l(_cl_lock);

			// get the list of all available URIs
			std::list<Node::URI> uri_list = node.getAll();

			// search for a match between URI and available convergence layer
			for (std::list<Node::URI>::const_iterator it = uri_list.begin(); it != uri_list.end(); ++it)
			{
				const Node::URI &uri = (*it);

				// search a matching convergence layer for this URI
				for (std::set<ConvergenceLayer*>::iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
				{
					ConvergenceLayer *cl = (*iter);
					if (cl->getDiscoveryProtocol() == uri.protocol)
					{
						cl->queue(node, job);

						// stop here, we queued the bundle already
						return;
					}
				}
			}

			// throw dial-up exception if there are P2P dial-up connections available
			if (node.hasDialup()) throw P2PDialupException();

			throw ConnectionNotAvailableException();
		}

		void ConnectionManager::queue(const dtn::net::BundleTransfer &job)
		{
			ibrcommon::MutexLock l(_node_lock);

			if (IBRCOMMON_LOGGER_LEVEL >= 50)
			{
				IBRCOMMON_LOGGER_DEBUG_TAG("ConnectionManager", 50) << "## node list ##" << IBRCOMMON_LOGGER_ENDL;
				for (nodemap::const_iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
				{
					const dtn::core::Node &n = (*iter).second;
					IBRCOMMON_LOGGER_DEBUG_TAG("ConnectionManager", 2) << n << IBRCOMMON_LOGGER_ENDL;
				}
			}

			IBRCOMMON_LOGGER_DEBUG_TAG("ConnectionManager", 50) << "search for node " << job.getNeighbor().getString() << IBRCOMMON_LOGGER_ENDL;

			// queue to a node
			const Node &n = getNode(job.getNeighbor());
			IBRCOMMON_LOGGER_DEBUG_TAG("ConnectionManager", 2) << "next hop: " << n << IBRCOMMON_LOGGER_ENDL;

			try {
				queue(n, job);
			} catch (const P2PDialupException&) {
				// trigger the dial-up connection
				dialup(n);

				// re-throw P2PDialupException
				throw;
			}
		}

		const std::set<dtn::core::Node> ConnectionManager::getNeighbors()
		{
			ibrcommon::MutexLock l(_node_lock);

			std::set<dtn::core::Node> ret;

			for (nodemap::const_iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
			{
				const Node &n = (*iter).second;
				if (n.isAvailable()) ret.insert( n );
			}

			return ret;
		}

		const dtn::core::Node ConnectionManager::getNeighbor(const dtn::data::EID &eid) throw (NeighborNotAvailableException)
		{
			ibrcommon::MutexLock l(_node_lock);
			const Node &n = getNode(eid);
			if (n.isAvailable()) return n;

			throw dtn::net::NeighborNotAvailableException();
		}

		bool ConnectionManager::isNeighbor(const dtn::core::Node &node)
		{
			try {
				ibrcommon::MutexLock l(_node_lock);
				const Node &n = getNode(node.getEID());
				if (n.isAvailable()) return true;
			} catch (const NeighborNotAvailableException&) { }

			return false;
		}

		void ConnectionManager::updateNeighbor(const Node &n)
		{
			discovered(n);
		}

		const std::string ConnectionManager::getName() const
		{
			return "ConnectionManager";
		}

		dtn::core::Node& ConnectionManager::getNode(const dtn::data::EID &eid) throw (NeighborNotAvailableException)
		{
			nodemap::iterator iter = _nodes.find(eid);
			if (iter == _nodes.end()) throw NeighborNotAvailableException("neighbor not found");
			return (*iter).second;
		}
	}
}
