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
#include "core/EventDispatcher.h"
#include "core/BundleEvent.h"
#include "core/BundleCore.h"

#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <typeinfo>

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
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::net::ConnectionEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::GlobalEvent>::add(this);

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
				_cl_protocols.clear();
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

		void ConnectionManager::raiseEvent(const dtn::core::NodeEvent &nodeevent) throw ()
		{
			ibrcommon::MutexLock l(_node_lock);
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
		}

		void ConnectionManager::raiseEvent(const dtn::core::TimeEvent &timeevent) throw ()
		{
			if (timeevent.getAction() == TIME_SECOND_TICK)
			{
				check_unavailable();
				check_autoconnect();
			}
		}

		void ConnectionManager::raiseEvent(const dtn::net::ConnectionEvent &connection) throw ()
		{
			switch (connection.getState())
			{
				case ConnectionEvent::CONNECTION_UP:
				{
					add(connection.getNode());
					break;
				}

				case ConnectionEvent::CONNECTION_DOWN:
				{
					remove(connection.getNode());
					break;
				}

				default:
					break;
			}
		}

		void ConnectionManager::raiseEvent(const dtn::core::GlobalEvent &global) throw ()
		{
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
		}

		void ConnectionManager::add(const dtn::core::Node &n) throw ()
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
					dtn::core::BundleCore::getInstance().addRoute(node.getEID(), fakeNode.getEID());

					node.remove((*iter));

					if(node.getAll().size() == 0)
						return;
				}

				updateNeighbor(node);

				return;
			}

			ibrcommon::MutexLock l(_node_lock);
			std::pair<nodemap::iterator,bool> ret = _nodes.insert( std::pair<dtn::data::EID, dtn::core::Node>(n.getEID(), n) );

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

			if (db.isAvailable() && !db.isAnnounced() && isReachable(db)) {
				db.setAnnounced(true);

				// announce the new node
				dtn::core::NodeEvent::raise(db, dtn::core::NODE_AVAILABLE);
			}
		}

		void ConnectionManager::remove(const dtn::core::Node &n) throw ()
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
			} catch (const NodeNotAvailableException&) { };
		}

		void ConnectionManager::add(ConvergenceLayer *cl)
		{
			ibrcommon::MutexLock l(_cl_lock);
			_cl.insert( cl );
			_cl_protocols.insert( cl->getDiscoveryProtocol() );
		}

		void ConnectionManager::remove(ConvergenceLayer *cl)
		{
			ibrcommon::MutexLock l(_cl_lock);
			_cl.erase( cl );

			// update protocols
			_cl_protocols.clear();
			for (std::set<ConvergenceLayer*>::const_iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
			{
				ConvergenceLayer &cl = (**iter);
				_cl_protocols.insert( cl.getDiscoveryProtocol() );
			}
		}

		void ConnectionManager::getStats(dtn::net::ConvergenceLayer::stats_data &data)
		{
			ibrcommon::MutexLock l(_cl_lock);
			for (std::set<ConvergenceLayer*>::const_iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
			{
				ConvergenceLayer &cl = (**iter);
				cl.getStats(data);
			}
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

		bool ConnectionManager::isReachable(const dtn::core::Node &node) throw ()
		{
			const std::list<Node::URI> urilist = node.getAll();

			for (std::list<Node::URI>::const_iterator uri_it = urilist.begin(); uri_it != urilist.end(); ++uri_it)
			{
				const Node::URI &uri = (*uri_it);

				if (uri.type == Node::NODE_P2P_DIALUP)
				{
					// lock P2P manager while iterating over them
					ibrcommon::MutexLock l(_dialup_lock);

					// check if there is a P2P link opportunity
					for (std::set<P2PDialupExtension*>::iterator iter = _dialups.begin(); iter != _dialups.end(); ++iter)
					{
						const P2PDialupExtension &p2pext = (**iter);
						if (p2pext.getProtocol() == uri.protocol)
						{
							// link opportunity found
							return true;
						}
					}
				}
				else
				{
					// lock convergence layers while iterating over them
					ibrcommon::MutexLock l(_cl_lock);

					// check if there is a link opportunity
					for (std::set<ConvergenceLayer*>::iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
					{
						const ConvergenceLayer &cl = (**iter);
						if (cl.getDiscoveryProtocol() == uri.protocol)
						{
							// link opportunity found
							return true;
						}
					}
				}
			}

			return false;
		}

		void ConnectionManager::check_available()
		{
			ibrcommon::MutexLock l(_node_lock);

			// search for outdated nodes
			for (nodemap::iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
			{
				dtn::core::Node &n = (*iter).second;
				if (n.isAnnounced()) continue;

				if (n.isAvailable() && isReachable(n)) {
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

				if ( !n.isAvailable() ||  !isReachable(n) ) {
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
			std::queue<dtn::core::Node> connect_nodes;

			dtn::data::Timeout interval = dtn::daemon::Configuration::getInstance().getNetwork().getAutoConnect();
			if (interval == 0) return;

			if (_next_autoconnect < dtn::utils::Clock::getTime())
			{
				// search for non-connected but available nodes
				ibrcommon::MutexLock l(_node_lock);
				for (nodemap::const_iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
				{
					const Node &n = (*iter).second;
					std::list<Node::URI> ul = n.get(Node::NODE_CONNECTED, Node::CONN_TCPIP);

					if (ul.empty() && n.isAvailable()  && isReachable(n))
					{
						connect_nodes.push(n);
					}
				}

				// set the next check time
				_next_autoconnect = dtn::utils::Clock::getTime() + interval;
			}

			while (!connect_nodes.empty())
			{
				try {
					open(connect_nodes.front());
				} catch (const ibrcommon::Exception&) {
					// ignore errors
				}
				connect_nodes.pop();
			}
		}

		void ConnectionManager::open(const dtn::core::Node &node) throw (ibrcommon::Exception)
		{
			const std::list<Node::URI> urilist = node.getAll();

			for (std::list<Node::URI>::const_iterator uri_it = urilist.begin(); uri_it != urilist.end(); ++uri_it)
			{
				const Node::URI &uri = (*uri_it);

				if (uri.type == Node::NODE_P2P_DIALUP)
				{
					// check if there is a P2P connection to establish
					ibrcommon::MutexLock l(_dialup_lock);
					for (std::set<P2PDialupExtension*>::iterator iter = _dialups.begin(); iter != _dialups.end(); ++iter)
					{
						P2PDialupExtension &p2pext = (**iter);

						if (uri.protocol == p2pext.getProtocol()) {
							// trigger connection set-up
							p2pext.connect(uri);
							throw P2PDialupException();
						}
					}
				}
				else
				{
					// lock convergence layers while iterating over them
					ibrcommon::MutexLock l(_cl_lock);

					// search for the right cl
					for (std::set<ConvergenceLayer*>::iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
					{
						ConvergenceLayer *cl = (*iter);
						if (cl->getDiscoveryProtocol() == uri.protocol)
						{
							cl->open(node);

							// stop here, we queued the bundle already
							return;
						}
					}
				}
			}

			throw dtn::net::ConnectionNotAvailableException();
		}

		void ConnectionManager::queue(dtn::net::BundleTransfer &job)
		{
			try {
				ibrcommon::MutexLock l(_node_lock);

				// queue to a node
				const Node &n = getNode(job.getNeighbor());

				// debug output
				IBRCOMMON_LOGGER_DEBUG_TAG("ConnectionManager", 2) << "next hop: " << n << IBRCOMMON_LOGGER_ENDL;

				// lock convergence layers while iterating over them
				{
					ibrcommon::MutexLock lcl(_cl_lock);

					// search a matching convergence layer for the desired path
					for (std::set<ConvergenceLayer*>::iterator iter = _cl.begin(); iter != _cl.end(); ++iter)
					{
						ConvergenceLayer *cl = (*iter);
						if (cl->getDiscoveryProtocol() == job.getProtocol())
						{
							cl->queue(n, job);

							// stop here, we queued the bundle already
							return;
						}
					}
				}

				// check if there is a P2P connection to establish
				{
					ibrcommon::MutexLock ldu(_dialup_lock);
					for (std::set<P2PDialupExtension*>::iterator iter = _dialups.begin(); iter != _dialups.end(); ++iter)
					{
						P2PDialupExtension &p2pext = (**iter);

						if (job.getProtocol() == p2pext.getProtocol()) {
							// matching P2P extension found
							// get P2P credentials
							const std::list<Node::URI> urilist = n.get(job.getProtocol());

							// check if already connected
							for (std::list<Node::URI>::const_iterator uri_it = urilist.begin(); uri_it != urilist.end(); ++uri_it)
							{
								const Node::URI &p2puri = (*uri_it);
								if (p2puri.type == Node::NODE_CONNECTED) {
									throw P2PDialupException();
								} else {
									// trigger connection set-up
									p2pext.connect(*uri_it);
									return;
								}
							}
						}
					}
				}

				// signal interruption of the transfer
				job.abort(dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
			} catch (const dtn::net::NodeNotAvailableException &ex) {
				// signal interruption of the transfer
				job.abort(dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
			}
		}

		const ConnectionManager::protocol_set ConnectionManager::getSupportedProtocols() throw ()
		{
			// lock convergence layers
			ibrcommon::MutexLock l(_cl_lock);
			return _cl_protocols;
		}

		const ConnectionManager::protocol_list ConnectionManager::getSupportedProtocols(const dtn::data::EID &peer) throw (NodeNotAvailableException)
		{
			protocol_list ret;

			const dtn::core::Node node = getNeighbor(peer);
			const std::list<Node::URI> protocols = node.getAll();

			// lock convergence layers while iterating over them
			ibrcommon::MutexLock l(_cl_lock);

			for (std::list<Node::URI>::const_iterator iter = protocols.begin(); iter != protocols.end(); ++iter)
			{
				const Node::URI &uri = (*iter);
				if (uri.type == Node::NODE_P2P_DIALUP || _cl_protocols.find(uri.protocol) != _cl_protocols.end())
				{
					ret.push_back(uri.protocol);
				}
			}

			return ret;
		}

		const std::set<dtn::core::Node> ConnectionManager::getNeighbors()
		{
			ibrcommon::MutexLock l(_node_lock);

			std::set<dtn::core::Node> ret;

			for (nodemap::const_iterator iter = _nodes.begin(); iter != _nodes.end(); ++iter)
			{
				const Node &n = (*iter).second;
				if (n.isAvailable() && isReachable(n)) ret.insert( n );
			}

			return ret;
		}

		const dtn::core::Node ConnectionManager::getNeighbor(const dtn::data::EID &eid) throw (NodeNotAvailableException)
		{
			ibrcommon::MutexLock l(_node_lock);
			const Node &n = getNode(eid);
			if (n.isAvailable() && isReachable(n)) return n;

			throw dtn::net::NodeNotAvailableException("Node is not reachable or not available.");
		}

		bool ConnectionManager::isNeighbor(const dtn::core::Node &node) throw ()
		{
			try {
				ibrcommon::MutexLock l(_node_lock);
				const Node &n = getNode(node.getEID());
				if (n.isAvailable() && isReachable(n)) return true;
			} catch (const NodeNotAvailableException&) { }

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

		dtn::core::Node& ConnectionManager::getNode(const dtn::data::EID &eid) throw (NodeNotAvailableException)
		{
			nodemap::iterator iter = _nodes.find(eid);
			if (iter == _nodes.end()) throw NodeNotAvailableException("node not found");
			return (*iter).second;
		}
	}
}
