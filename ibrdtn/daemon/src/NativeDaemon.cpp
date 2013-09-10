/*
 * NativeDaemon.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
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
#include "NativeDaemon.h"

#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/link/LinkManager.h>
#include <ibrdtn/utils/Clock.h>
#include <ibrdtn/utils/Utils.h>
#include <list>

#include "storage/BundleStorage.h"
#include "storage/BundleSeeker.h"
#include "storage/MemoryBundleStorage.h"
#include "storage/SimpleBundleStorage.h"

#include "core/BundleCore.h"
#include "net/ConnectionManager.h"
#include "core/FragmentManager.h"
#include "core/Node.h"
#include "core/EventSwitch.h"
#include "core/EventDispatcher.h"

#include "routing/BaseRouter.h"
#include "routing/StaticRoutingExtension.h"
#include "routing/NeighborRoutingExtension.h"
#include "routing/SchedulingBundleIndex.h"
#include "routing/epidemic/EpidemicRoutingExtension.h"
#include "routing/prophet/ProphetRoutingExtension.h"
#include "routing/flooding/FloodRoutingExtension.h"

#include "core/GlobalEvent.h"
#include "net/BundleReceivedEvent.h"
#include "net/TransferCompletedEvent.h"
#include "net/TransferAbortedEvent.h"
#include "core/BundleGeneratedEvent.h"
#include "core/BundleExpiredEvent.h"
#include "routing/QueueBundleEvent.h"
#include "routing/RequeueBundleEvent.h"
#include "core/TimeAdjustmentEvent.h"

#include "net/UDPConvergenceLayer.h"
#include "net/TCPConvergenceLayer.h"
#include "net/FileConvergenceLayer.h"

#ifdef HAVE_SYS_INOTIFY_H
#include "net/FileMonitor.h"
#endif

#include "net/DatagramConvergenceLayer.h"
#include "net/UDPDatagramService.h"

#ifdef HAVE_SQLITE
#include "storage/SQLiteBundleStorage.h"
#endif

#ifdef HAVE_LIBCURL
#include "net/HTTPConvergenceLayer.h"
#endif

#ifdef HAVE_LOWPAN_SUPPORT
#include "net/LOWPANConvergenceLayer.h"
#include "net/LOWPANDatagramService.h"
#endif

#include "net/IPNDAgent.h"

#include "api/ApiServer.h"

#include "Configuration.h"
#include "EchoWorker.h"
#include "CapsuleWorker.h"
#include "DTNTPWorker.h"
#include "DevNull.h"
#include "Component.h"

#ifdef WITH_WIFIP2P
#include "net/WifiP2PManager.h"
#endif

#ifdef HAVE_VMIME
#include "net/EMailConvergenceLayer.h"
#endif

#ifdef WITH_BUNDLE_SECURITY
#include "security/SecurityManager.h"
#include "security/SecurityKeyManager.h"
#endif

#ifdef WITH_TLS
#include "security/SecurityCertificateManager.h"
#endif

#ifdef WITH_DHT_NAMESERVICE
#include "net/DHTNameService.h"
#endif

#include "Debugger.h"
#include "core/EventDebugger.h"

/** events **/
#include "core/NodeEvent.h"
#include "core/GlobalEvent.h"
#include "core/CustodyEvent.h"
#include "routing/QueueBundleEvent.h"
#include "net/BundleReceivedEvent.h"
#include "net/TransferAbortedEvent.h"
#include "net/TransferCompletedEvent.h"
#include "net/ConnectionEvent.h"

#define UNIT_MB * 1048576

namespace dtn
{
	namespace daemon
	{
		NativeDaemonCallback::~NativeDaemonCallback()
		{
		}

		NativeEventCallback::~NativeEventCallback()
		{
		}

		const std::string NativeDaemon::TAG = "NativeDaemon";

		NativeDaemon::NativeDaemon(NativeDaemonCallback *statecb, NativeEventCallback *eventcb)
		 : _runlevel(RUNLEVEL_ZERO), _statecb(statecb), _eventcb(eventcb), _event_loop(NULL)
		{

		}

		NativeDaemon::~NativeDaemon()
		{
		}

		std::vector<std::string> NativeDaemon::getVersion() const throw ()
		{
			std::vector<std::string> ret;
			ret.push_back(VERSION);
			ret.push_back(BUILD_NUMBER);
			return ret;
		}

		void NativeDaemon::clearStorage() const throw ()
		{
			dtn::core::BundleCore::getInstance().getStorage().clear();
		}

		void NativeDaemon::startDiscovery() const throw ()
		{
			dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_START_DISCOVERY);
		}

		void NativeDaemon::stopDiscovery() const throw ()
		{
			dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_STOP_DISCOVERY);
		}

		void NativeDaemon::bindEvents()
		{
			if (_eventcb != NULL) {
				// bind to several events
				dtn::core::EventDispatcher<dtn::core::NodeEvent>::add(this);
				dtn::core::EventDispatcher<dtn::core::GlobalEvent>::add(this);
				dtn::core::EventDispatcher<dtn::core::CustodyEvent>::add(this);
				dtn::core::EventDispatcher<dtn::net::BundleReceivedEvent>::add(this);
				dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::add(this);
				dtn::core::EventDispatcher<dtn::net::TransferCompletedEvent>::add(this);
				dtn::core::EventDispatcher<dtn::net::ConnectionEvent>::add(this);
				dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(this);
			}
		}

		void NativeDaemon::unbindEvents()
		{
			if (_eventcb != NULL) {
				// unbind to events
				dtn::core::EventDispatcher<dtn::core::NodeEvent>::remove(this);
				dtn::core::EventDispatcher<dtn::core::GlobalEvent>::remove(this);
				dtn::core::EventDispatcher<dtn::core::CustodyEvent>::remove(this);
				dtn::core::EventDispatcher<dtn::net::BundleReceivedEvent>::remove(this);
				dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::remove(this);
				dtn::core::EventDispatcher<dtn::net::TransferCompletedEvent>::remove(this);
				dtn::core::EventDispatcher<dtn::net::ConnectionEvent>::remove(this);
				dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(this);
			}
		}

		void NativeDaemon::addEventData(const dtn::data::Bundle &b, std::vector<std::string> &data) const
		{
			// add the bundle data
			data.push_back("Source: " + b.source.getString());
			data.push_back("Timestamp: " + b.timestamp.toString());
			data.push_back("Sequencenumber: " + b.sequencenumber.toString());
			data.push_back("Lifetime: " + b.lifetime.toString());
			data.push_back("Procflags: " + b.procflags.toString());

			// add the destination eid
			data.push_back("Destination: " + b.destination.getString());

			if (b.get(dtn::data::PrimaryBlock::FRAGMENT))
			{
				// add fragmentation values
				data.push_back("Appdatalength: " + b.appdatalength.toString());
				data.push_back("Fragmentoffset: " + b.fragmentoffset.toString());
			}
		}

		void NativeDaemon::addEventData(const dtn::data::MetaBundle &b, std::vector<std::string> &data) const
		{
			// add the bundle data
			data.push_back("Source: " + b.source.getString());
			data.push_back("Timestamp: " + b.timestamp.toString());
			data.push_back("Sequencenumber: " + b.sequencenumber.toString());
			data.push_back("Lifetime: " + b.lifetime.toString());
			data.push_back("Procflags: " + b.procflags.toString());

			// add the destination eid
			data.push_back("Destination: " + b.destination.getString());

			if (b.get(dtn::data::PrimaryBlock::FRAGMENT))
			{
				// add fragmentation values
				data.push_back("Appdatalength: " + b.appdatalength.toString());
				data.push_back("Fragmentoffset: " + b.offset.toString());
			}
		}

		void NativeDaemon::addEventData(const dtn::data::BundleID &b, std::vector<std::string> &data) const
		{
			// add the bundle data
			data.push_back("Source: " + b.source.getString());
			data.push_back("Timestamp: " + b.timestamp.toString());
			data.push_back("Sequencenumber: " + b.sequencenumber.toString());

			if (b.fragment)
			{
				// add fragmentation values
				data.push_back("Fragmentoffset: " + b.offset.toString());
			}
		}

		void NativeDaemon::raiseEvent(const Event *evt) throw ()
		{
			const std::string event = evt->getName();
			std::string action;
			std::vector<std::string> data;

			try {
				const dtn::core::NodeEvent &node = dynamic_cast<const dtn::core::NodeEvent&>(*evt);

				// set action
				switch (node.getAction())
				{
				case dtn::core::NODE_AVAILABLE:
					action = "available";
					break;
				case dtn::core::NODE_UNAVAILABLE:
					action = "unavailable";
					break;
				case dtn::core::NODE_DATA_ADDED:
					action = "data_added";
					break;
				case dtn::core::NODE_DATA_REMOVED:
					action = "data_removed";
					break;
				default:
					break;
				}

				// add the node eid
				data.push_back("EID: " + node.getNode().getEID().getString());
			} catch (const std::bad_cast&) { };

			try {
				const dtn::core::GlobalEvent &global = dynamic_cast<const dtn::core::GlobalEvent&>(*evt);

				// set action
				switch (global.getAction())
				{
				case dtn::core::GlobalEvent::GLOBAL_BUSY:
					action = "busy";
					break;
				case dtn::core::GlobalEvent::GLOBAL_IDLE:
					action = "idle";
					break;
				case dtn::core::GlobalEvent::GLOBAL_RESUME:
					action = "resume";
					break;
				case dtn::core::GlobalEvent::GLOBAL_RELOAD:
					action = "reload";
					break;
				case dtn::core::GlobalEvent::GLOBAL_SHUTDOWN:
					action = "shutdown";
					break;
				case dtn::core::GlobalEvent::GLOBAL_SUSPEND:
					action = "suspend";
					break;
				case dtn::core::GlobalEvent::GLOBAL_INTERNET_AVAILABLE:
					action = "internet available";
					break;
				case dtn::core::GlobalEvent::GLOBAL_INTERNET_UNAVAILABLE:
					action = "internet unavailable";
					break;
				case dtn::core::GlobalEvent::GLOBAL_START_DISCOVERY:
					action = "discovery start";
					break;
				case dtn::core::GlobalEvent::GLOBAL_STOP_DISCOVERY:
					action = "discovery stop";
					break;
				default:
					break;
				}
			} catch (const std::bad_cast&) { };

			try {
				const dtn::net::BundleReceivedEvent &received = dynamic_cast<const dtn::net::BundleReceivedEvent&>(*evt);

				// set action
				action = "received";

				data.push_back("Peer: " + received.peer.getString());
				data.push_back("Local: " + (received.fromlocal ? std::string("true") : std::string("false")));

				// add bundle data
				addEventData(received.bundle, data);
			} catch (const std::bad_cast&) { };

			try {
				const dtn::core::CustodyEvent &custody = dynamic_cast<const dtn::core::CustodyEvent&>(*evt);

				// set action
				switch (custody.getAction())
				{
				case dtn::core::CUSTODY_ACCEPT:
					action = "accept";
					break;
				case dtn::core::CUSTODY_REJECT:
					action = "reject";
					break;
				default:
					break;
				}

				// add bundle data
				addEventData(custody.getBundle(), data);
			} catch (const std::bad_cast&) { };

			try {
				const dtn::net::TransferAbortedEvent &aborted = dynamic_cast<const dtn::net::TransferAbortedEvent&>(*evt);

				// set action
				action = "aborted";

				data.push_back("Peer: " + aborted.getPeer().getString());

				// add bundle data
				addEventData(aborted.getBundleID(), data);
			} catch (const std::bad_cast&) { };

			try {
				const dtn::net::TransferCompletedEvent &completed = dynamic_cast<const dtn::net::TransferCompletedEvent&>(*evt);

				// set action
				action = "completed";

				data.push_back("Peer: " + completed.getPeer().getString());

				// add bundle data
				addEventData(completed.getBundle(), data);
			} catch (const std::bad_cast&) { };

			try {
				const dtn::net::ConnectionEvent &connection = dynamic_cast<const dtn::net::ConnectionEvent&>(*evt);

				// set action
				switch (connection.state)
				{
				case dtn::net::ConnectionEvent::CONNECTION_UP:
					action = "up";
					break;
				case dtn::net::ConnectionEvent::CONNECTION_DOWN:
					action = "down";
					break;
				case dtn::net::ConnectionEvent::CONNECTION_SETUP:
					action = "setup";
					break;
				case dtn::net::ConnectionEvent::CONNECTION_TIMEOUT:
					action = "timeout";
					break;
				default:
					break;
				}

				// write the peer eid
				data.push_back("Peer: " + connection.peer.getString());
			} catch (const std::bad_cast&) { };

			try {
				const dtn::routing::QueueBundleEvent &queued = dynamic_cast<const dtn::routing::QueueBundleEvent&>(*evt);

				// set action
				action = "queued";

				// add bundle data
				addEventData(queued.bundle, data);
			} catch (const std::bad_cast&) { };

			// signal event to the callback interface
			_eventcb->eventRaised(event, action, data);
		}

		std::string NativeDaemon::getLocalUri() const throw ()
		{
			return dtn::core::BundleCore::local.getString();
		}

		NativeStats NativeDaemon::getStats() throw ()
		{
			NativeStats ret;

			ret.uptime = dtn::utils::Clock::getUptime().get<size_t>();
			ret.timestamp = dtn::utils::Clock::getTime().get<size_t>();
			ret.neighbors = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors().size();
			ret.storage_size = dtn::core::BundleCore::getInstance().getStorage().size();

			ret.time_offset = dtn::utils::Clock::toDouble(dtn::utils::Clock::getOffset());
			ret.time_rating = dtn::utils::Clock::getRating();
			ret.time_adjustments = dtn::core::EventDispatcher<dtn::core::TimeAdjustmentEvent>::getCounter();

			ret.bundles_stored = dtn::core::BundleCore::getInstance().getStorage().count();
			ret.bundles_expired = dtn::core::EventDispatcher<dtn::core::BundleExpiredEvent>::getCounter();
			ret.bundles_generated = dtn::core::EventDispatcher<dtn::core::BundleGeneratedEvent>::getCounter();
			ret.bundles_received = dtn::core::EventDispatcher<dtn::net::BundleReceivedEvent>::getCounter();
			ret.bundles_transmitted = dtn::core::EventDispatcher<dtn::net::TransferCompletedEvent>::getCounter();
			ret.bundles_aborted = dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::getCounter();
			ret.bundles_requeued = dtn::core::EventDispatcher<dtn::routing::RequeueBundleEvent>::getCounter();
			ret.bundles_queued = dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::getCounter();

			using dtn::net::ConnectionManager;
			using dtn::net::ConvergenceLayer;

			ConnectionManager::stats_list list = dtn::core::BundleCore::getInstance().getConnectionManager().getStats();

			for (ConnectionManager::stats_list::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
				const ConnectionManager::stats_pair &pair = (*iter);
				const ConvergenceLayer::stats_map &map = pair.second;

				for (ConvergenceLayer::stats_map::const_iterator map_it = map.begin(); map_it != map.end(); ++map_it) {
					std::string tag = dtn::core::Node::toString(pair.first) + "|" + (*map_it).first;
					ret.addData(tag, (*map_it).second);
				}
			}

			return ret;
		}

		NativeNode NativeDaemon::getInfo(const std::string &neighbor_eid) const throw (NativeDaemonException)
		{
			NativeNode nn(neighbor_eid);

			dtn::data::EID eid(neighbor_eid);
			try {
				dtn::core::Node n = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbor(eid);
				dtn::core::Node::URI uri = n.getAll().front();
				switch (uri.type) {
				case dtn::core::Node::NODE_UNAVAILABLE:
					throw NativeDaemonException("Node is not available");
					break;

				case dtn::core::Node::NODE_CONNECTED:
					nn.type = NativeNode::NODE_CONNECTED;
					break;

				case dtn::core::Node::NODE_DISCOVERED:
					nn.type = NativeNode::NODE_DISCOVERED;
					break;

				case dtn::core::Node::NODE_STATIC_GLOBAL:
					nn.type = NativeNode::NODE_INTERNET;
					break;

				case dtn::core::Node::NODE_STATIC_LOCAL:
					nn.type = NativeNode::NODE_STATIC;
					break;

				case dtn::core::Node::NODE_DHT_DISCOVERED:
					nn.type = NativeNode::NODE_DISCOVERED;
					break;

				case dtn::core::Node::NODE_P2P_DIALUP:
					nn.type = NativeNode::NODE_P2P;
					break;
				}
			} catch (const NeighborNotAvailableException &ex) {
				throw NativeDaemonException(ex.what());
			}

			return nn;
		}

		std::vector<std::string> NativeDaemon::getNeighbors() const throw ()
		{
			std::vector<std::string> ret;

			// get neighbors
			const std::set<dtn::core::Node> nlist = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();
			for (std::set<dtn::core::Node>::const_iterator iter = nlist.begin(); iter != nlist.end(); ++iter)
			{
				const dtn::core::Node &n = (*iter);
				ret.push_back(n.getEID().getString());
			}

			return ret;
		}

		void NativeDaemon::addConnection(std::string eid, std::string protocol, std::string address, std::string service, bool local) const throw ()
		{
			dtn::core::Node n(eid);
			dtn::core::Node::Type t = dtn::core::Node::NODE_STATIC_GLOBAL;

			if (local) t = dtn::core::Node::NODE_STATIC_LOCAL;

			if (protocol == "tcp")
			{
				std::string uri = "ip=" + address + ";port=" + service + ";";
				n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_TCPIP, uri, 0, 10));
				dtn::core::BundleCore::getInstance().getConnectionManager().add(n);
			}
			else if (protocol == "udp")
			{
				std::string uri = "ip=" + address + ";port=" + service + ";";
				n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_UDPIP, uri, 0, 10));
				dtn::core::BundleCore::getInstance().getConnectionManager().add(n);
			}
			else if (protocol == "file")
			{
				n.add(dtn::core::Node::URI(dtn::core::Node::NODE_STATIC_LOCAL, dtn::core::Node::CONN_FILE, address, 0, 10));
				dtn::core::BundleCore::getInstance().getConnectionManager().add(n);
			}
		}

		void NativeDaemon::removeConnection(std::string eid, std::string protocol, std::string address, std::string service, bool local) const throw ()
		{
			dtn::core::Node n(eid);
			dtn::core::Node::Type t = dtn::core::Node::NODE_STATIC_GLOBAL;

			if (local) t = dtn::core::Node::NODE_STATIC_LOCAL;

			if (protocol == "tcp")
			{
				std::string uri = "ip=" + address + ";port=" + service + ";";
				n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_TCPIP, uri, 0, 10));
				dtn::core::BundleCore::getInstance().getConnectionManager().remove(n);
			}
			else if (protocol == "udp")
			{
				std::string uri = "ip=" + address + ";port=" + service + ";";
				n.add(dtn::core::Node::URI(t, dtn::core::Node::CONN_UDPIP, uri, 0, 10));
				dtn::core::BundleCore::getInstance().getConnectionManager().remove(n);
			}
			else if (protocol == "file")
			{
				n.add(dtn::core::Node::URI(dtn::core::Node::NODE_STATIC_LOCAL, dtn::core::Node::CONN_FILE, address, 0, 10));
				dtn::core::BundleCore::getInstance().getConnectionManager().remove(n);
			}
		}

		void NativeDaemon::initiateConnection(std::string eid)
		{
			dtn::data::EID neighbor(eid);
			dtn::net::ConnectionManager &cm = dtn::core::BundleCore::getInstance().getConnectionManager();

			try {
				const dtn::core::Node n = cm.getNeighbor(neighbor);
				cm.open(n);
			} catch (const ibrcommon::Exception&) {
				// ignore errors
			}
		}

		void NativeDaemon::setLogging(const std::string &defaultTag, int logLevel) const throw ()
		{
			/**
			 * setup logging capabilities
			 */

			// set default logging
			unsigned char logsys = ibrcommon::Logger::LOGGER_EMERG | ibrcommon::Logger::LOGGER_ALERT | ibrcommon::Logger::LOGGER_CRIT | ibrcommon::Logger::LOGGER_ERR;

			// activate info messages and warnings
			if (logLevel > 0) {
				logsys |= ibrcommon::Logger::LOGGER_WARNING;
				logsys |= ibrcommon::Logger::LOGGER_INFO;
			}

			// activate frequent notice messages
			if (logLevel > 1) {
				logsys |= ibrcommon::Logger::LOGGER_NOTICE;
			}

			// activate debugging
			if (logLevel > 2) {
				logsys |= ibrcommon::Logger::LOGGER_DEBUG;
			}

			// set default logging tag
			ibrcommon::Logger::setDefaultTag(defaultTag);

			// enable logging to Android's logcat
			ibrcommon::Logger::enableSyslog("ibrdtn-daemon", 0, 0, logsys);
		}

		void NativeDaemon::setConfigFile(const std::string &path)
		{
			_config_file = ibrcommon::File(path);

			// load the configuration file
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance(true);
			conf.load(_config_file.getPath());

			// reload bundle core configuration
			dtn::core::BundleCore::getInstance().onConfigurationChanged(conf);

#ifdef WITH_BUNDLE_SECURITY
			// initialize the key manager for the security extensions
			dtn::security::SecurityKeyManager::getInstance().onConfigurationChanged( conf );
#endif
		}

		/**
		 * Set the path to the log file
		 */
		void NativeDaemon::setLogFile(const std::string &path, int logLevel) const throw ()
		{
			// logging options
			unsigned char logopts = ibrcommon::Logger::LOG_DATETIME | ibrcommon::Logger::LOG_LEVEL | ibrcommon::Logger::LOG_TAG;

			// set default logging
			unsigned char logsys = ibrcommon::Logger::LOGGER_EMERG | ibrcommon::Logger::LOGGER_ALERT | ibrcommon::Logger::LOGGER_CRIT | ibrcommon::Logger::LOGGER_ERR;

			// deactivate logging if logLevel is below zero
			if (logLevel < 0) {
				logsys = 0;
			}

			// activate info messages and warnings
			if (logLevel > 0) {
				logsys |= ibrcommon::Logger::LOGGER_WARNING;
				logsys |= ibrcommon::Logger::LOGGER_INFO;
			}

			// activate frequent notice messages
			if (logLevel > 1) {
				logsys |= ibrcommon::Logger::LOGGER_NOTICE;
			}

			// activate debugging
			if (logLevel > 2) {
				logsys |= ibrcommon::Logger::LOGGER_DEBUG;
			}

			const ibrcommon::File lf(path);
			ibrcommon::Logger::setLogfile(lf, logsys, logopts);
		}

		DaemonRunLevel NativeDaemon::getRunLevel() const throw ()
		{
			return _runlevel;
		}

		void NativeDaemon::init(DaemonRunLevel rl) throw (NativeDaemonException)
		{
			ibrcommon::MutexLock l(_runlevel_cond);
			if (_runlevel < rl) {
				for (; _runlevel < rl; _runlevel = DaemonRunLevel(_runlevel + 1)) {
					init_up(DaemonRunLevel(_runlevel + 1));
					_runlevel_cond.signal(true);
				}
			} else if (_runlevel > rl) {
				for (; _runlevel >= rl; _runlevel = DaemonRunLevel(_runlevel - 1)) {
					init_down(_runlevel);
					_runlevel_cond.signal(true);
				}
			}
		}

		void NativeDaemon::init_up(DaemonRunLevel rl) throw (NativeDaemonException)
		{
			switch (rl) {
			case RUNLEVEL_ZERO:
				// nothing to do here
				break;

			case RUNLEVEL_CORE:
				init_core();
				break;

			case RUNLEVEL_STORAGE:
				init_storage();
				break;

			case RUNLEVEL_ROUTING:
				init_routing();
				break;

			case RUNLEVEL_API:
				init_api();
				break;

			case RUNLEVEL_NETWORK:
				init_network();
				break;

			case RUNLEVEL_ROUTING_EXTENSIONS:
				init_routing_extensions();
				break;
			}

			/**
			 * initialize all components of this runlevel
			 */
			component_list &components = _components[rl];
			for (component_list::iterator it = components.begin(); it != components.end(); ++it)
			{
				(*it)->initialize();
			}

			/**
			 * start-up all components of this runlevel
			 */
			for (component_list::iterator it = components.begin(); it != components.end(); ++it)
			{
				(*it)->startup();
			}

			if (_statecb != NULL) {
				_statecb->levelChanged(rl);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeDaemon::TAG, 5) << "runlevel " << rl << " reached" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeDaemon::init_down(DaemonRunLevel rl) throw (NativeDaemonException)
		{
			/**
			 * shutdown all components of this runlevel
			 */
			component_list &components = _components[rl];
			for (component_list::iterator it = components.begin(); it != components.end(); ++it)
			{
				(*it)->terminate();
			}

			switch (rl) {
			case RUNLEVEL_ZERO:
				// nothing to do here
				break;

			case RUNLEVEL_CORE:
				shutdown_core();
				break;

			case RUNLEVEL_STORAGE:
				shutdown_storage();
				break;

			case RUNLEVEL_ROUTING:
				shutdown_routing();
				break;

			case RUNLEVEL_API:
				shutdown_api();
				break;

			case RUNLEVEL_NETWORK:
				shutdown_network();
				break;

			case RUNLEVEL_ROUTING_EXTENSIONS:
				shutdown_routing_extensions();
				break;
			}

			/**
			 * delete all components of this runlevel
			 */
			for (component_list::iterator it = components.begin(); it != components.end(); ++it)
			{
				delete (*it);
			}
			components.clear();

			if (_statecb != NULL) {
				_statecb->levelChanged(rl);
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NativeDaemon::TAG, 5) << "runlevel " << rl << " reached" << IBRCOMMON_LOGGER_ENDL;
		}

		void NativeDaemon::wait(DaemonRunLevel rl) throw ()
		{
			try {
				ibrcommon::MutexLock l(_runlevel_cond);
				while (_runlevel != rl) _runlevel_cond.wait();
			} catch (const ibrcommon::Conditional::ConditionalAbortException&) {
				// wait aborted
			}
		}

		void NativeDaemon::init_core() throw (NativeDaemonException)
		{
			// enable link manager
			ibrcommon::LinkManager::initialize();

			// get the core
			dtn::core::BundleCore &core = dtn::core::BundleCore::getInstance();

			// reset and get configuration
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();

			IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "IBR-DTN daemon " << conf.version() << IBRCOMMON_LOGGER_ENDL;

			// load configuration
			if (_config_file.exists()) {
				conf.load(_config_file.getPath());
			} else {
				conf.load();
			}

			// greeting
			if (conf.getDebug().enabled())
			{
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "debug level set to " << conf.getDebug().level() << IBRCOMMON_LOGGER_ENDL;
			}

			try {
				const ibrcommon::File &lf = conf.getLogger().getLogfile();
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "use logfile for output: " << lf.getPath() << IBRCOMMON_LOGGER_ENDL;
			} catch (const dtn::daemon::Configuration::ParameterNotSetException&) { };

			if (conf.getDaemon().getThreads() > 1)
			{
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Parallel event processing enabled using " << conf.getDaemon().getThreads() << " processes." << IBRCOMMON_LOGGER_ENDL;
			}

			// initialize the event switch
			dtn::core::EventSwitch::getInstance().initialize();

			// listen to events
			bindEvents();

			// create a thread for the main loop
			_event_loop = new NativeEventLoop(*this);

			// start the event switch
			_event_loop->start();

#ifdef WITH_TLS
			/* enable TLS support */
			if ( conf.getSecurity().doTLS() )
			{
				_components[RUNLEVEL_CORE].push_back(new dtn::security::SecurityCertificateManager());
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "TLS security enabled" << IBRCOMMON_LOGGER_ENDL;
			}
#endif

#ifdef WITH_BUNDLE_SECURITY
			// initialize the key manager for the security extensions
			dtn::security::SecurityKeyManager::getInstance().onConfigurationChanged( conf );
#endif

			// initialize core component
			core.initialize();
		}

		void NativeDaemon::shutdown_core() throw (NativeDaemonException)
		{
			// shutdown the event switch
			_event_loop->stop();
			_event_loop->join();

			delete _event_loop;
			_event_loop = NULL;

			// unlisten to events
			unbindEvents();

			// get the core
			dtn::core::BundleCore &core = dtn::core::BundleCore::getInstance();

			// shutdown the core component
			core.terminate();

#ifdef WITH_BUNDLE_SECURITY
			// reset configuration
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance(true);

			// initialize the key manager for the security extensions
			dtn::security::SecurityKeyManager::getInstance().onConfigurationChanged( conf );
#endif
		}

		void NativeDaemon::init_storage() throw (NativeDaemonException)
		{
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();

			// create a storage for bundles
			dtn::storage::BundleStorage *storage = NULL;

#ifdef HAVE_SQLITE
			if (conf.getStorage() == "sqlite")
			{
				try {
					// new methods for blobs
					ibrcommon::File path = conf.getPath("storage");

					// create workdir if needed
					if (!path.exists())
					{
						ibrcommon::File::createDirectory(path);
					}

					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "using sqlite bundle storage in " << path.getPath() << IBRCOMMON_LOGGER_ENDL;


					dtn::storage::SQLiteBundleStorage *sbs;
					if(conf.getUsePersistentBundleSets() == "yes")
					{
						sbs = new dtn::storage::SQLiteBundleStorage(path, conf.getLimit("storage"),true);
						IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "using persistent bundlesets" << IBRCOMMON_LOGGER_ENDL;
					} else {
						sbs = new dtn::storage::SQLiteBundleStorage(path, conf.getLimit("storage"),false );
					}

					_components[RUNLEVEL_STORAGE].push_back(sbs);
					storage = sbs;
				} catch (const dtn::daemon::Configuration::ParameterNotSetException&) {
					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "storage for bundles" << IBRCOMMON_LOGGER_ENDL;
					throw NativeDaemonException("initialization of the bundle storage failed");
				}
			}


#endif

			if ((conf.getStorage() == "simple") || (conf.getStorage() == "default"))
			{
				// default behavior if no bundle storage is set
				try {
					// new methods for blobs
					ibrcommon::File path = conf.getPath("storage");

					// create workdir if needed
					if (!path.exists())
					{
						ibrcommon::File::createDirectory(path);
					}

					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "using simple bundle storage in " << path.getPath() << IBRCOMMON_LOGGER_ENDL;
					dtn::storage::SimpleBundleStorage *sbs = new dtn::storage::SimpleBundleStorage(path, conf.getLimit("storage"), static_cast<unsigned int>(conf.getLimit("storage_buffer")));
					_components[RUNLEVEL_STORAGE].push_back(sbs);
					storage = sbs;
				} catch (const dtn::daemon::Configuration::ParameterNotSetException&) {
					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "using bundle storage in memory-only mode" << IBRCOMMON_LOGGER_ENDL;
					dtn::storage::MemoryBundleStorage *sbs = new dtn::storage::MemoryBundleStorage(conf.getLimit("storage"));
					_components[RUNLEVEL_STORAGE].push_back(sbs);
					storage = sbs;
				}
			}

			if (storage == NULL)
			{
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "bundle storage module \"" << conf.getStorage() << "\" do not exists!" << IBRCOMMON_LOGGER_ENDL;
				throw NativeDaemonException("bundle storage not available");
			}

			// set the storage in the core
			dtn::core::BundleCore::getInstance().setStorage(storage);

			// use scheduling?
			if (dtn::daemon::Configuration::getInstance().getNetwork().doScheduling())
			{
				// create a new bundle index
				dtn::storage::BundleIndex *bundle_index = new dtn::routing::SchedulingBundleIndex();

				// attach index to the storage
				storage->attach(bundle_index);

				// set this bundle index as default bundle seeker which is used to find bundles to transfer
				dtn::core::BundleCore::getInstance().setSeeker( bundle_index );

				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Enable extended bundle index for scheduling" << IBRCOMMON_LOGGER_ENDL;
			} else {
				// set the storage as default seeker
				dtn::core::BundleCore::getInstance().setSeeker( storage );
			}

			/**
			 * initialize blob storage mechanism
			 */
			try {
				// the configured BLOB path
				ibrcommon::File blob_path = conf.getPath("blob");

				// check if the BLOB path exists
				if (!blob_path.exists()) {
					// try to create the BLOB path
					ibrcommon::File::createDirectory(blob_path);
				}

				if (blob_path.exists() && blob_path.isDirectory())
				{
					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "using BLOB path: " << blob_path.getPath() << IBRCOMMON_LOGGER_ENDL;
					ibrcommon::BLOB::changeProvider(new ibrcommon::FileBLOBProvider(blob_path), true);
				}
				else
				{
					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, warning) << "BLOB path exists, but is not a directory! Fallback to memory based mode." << IBRCOMMON_LOGGER_ENDL;
					ibrcommon::BLOB::changeProvider(new ibrcommon::MemoryBLOBProvider(), true);
				}
			} catch (const dtn::daemon::Configuration::ParameterNotSetException&) { }
		}

		void NativeDaemon::shutdown_storage() const throw (NativeDaemonException)
		{
			// reset BLOB provider to memory based
			ibrcommon::BLOB::changeProvider(new ibrcommon::MemoryBLOBProvider(), true);

			// set the seeker in the core to NULL
			dtn::core::BundleCore::getInstance().setSeeker(NULL);

			// set the storage in the core to NULL
			dtn::core::BundleCore::getInstance().setStorage(NULL);
		}

		void NativeDaemon::init_routing() throw (NativeDaemonException)
		{
			// create the base router
			dtn::routing::BaseRouter *router = new dtn::routing::BaseRouter();

			// make the router globally available
			dtn::core::BundleCore::getInstance().setRouter(router);

			// add the router to the components list
			_components[RUNLEVEL_ROUTING].push_back(router);
		}

		void NativeDaemon::shutdown_routing() const throw (NativeDaemonException)
		{
			// set the global router to NULL
			dtn::core::BundleCore::getInstance().setRouter(NULL);
		}

		void NativeDaemon::init_api() throw (NativeDaemonException)
		{
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();

			// create event debugger
			_components[RUNLEVEL_API].push_back( new dtn::core::EventDebugger() );

			// Debugger
			_apps.push_back( new dtn::daemon::Debugger() );

			// add echo module
			_apps.push_back( new dtn::daemon::EchoWorker() );

			// add bundle-in-bundle endpoint
			_apps.push_back( new dtn::daemon::CapsuleWorker() );

			// add DT-NTP worker
			_apps.push_back( new dtn::daemon::DTNTPWorker() );

			// add DevNull module
			_apps.push_back( new dtn::daemon::DevNull() );

			if (conf.getNetwork().doFragmentation())
			{
				// manager class for fragmentations
				_components[RUNLEVEL_API].push_back( new dtn::core::FragmentManager() );
			}

#ifndef ANDROID
			if (conf.doAPI())
			{
		 		try {
					ibrcommon::File socket = conf.getAPISocket();

					try {
						// use unix domain sockets for API
						_components[RUNLEVEL_API].push_back(new dtn::api::ApiServer(socket));
						IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "API initialized using unix domain socket: " << socket.getPath() << IBRCOMMON_LOGGER_ENDL;
					} catch (const ibrcommon::socket_exception&) {
						IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Unable to bind to unix domain socket " << socket.getPath() << ". API not initialized!" << IBRCOMMON_LOGGER_ENDL;
						exit(-1);
					}
				} catch (const dtn::daemon::Configuration::ParameterNotSetException&) {
					dtn::daemon::Configuration::NetConfig apiconf = conf.getAPIInterface();

					try {
						// instance a API server, first create a socket
						_components[RUNLEVEL_API].push_back(new dtn::api::ApiServer(apiconf.iface, apiconf.port));
						IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "API initialized using tcp socket: " << apiconf.iface.toString() << ":" << apiconf.port << IBRCOMMON_LOGGER_ENDL;
					} catch (const ibrcommon::socket_exception&) {
						IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Unable to bind to " << apiconf.iface.toString() << ":" << apiconf.port << ". API not initialized!" << IBRCOMMON_LOGGER_ENDL;
						exit(-1);
					}
				};
			}
			else
			{
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "API disabled" << IBRCOMMON_LOGGER_ENDL;
			}
#endif
		}

		void NativeDaemon::shutdown_api() throw (NativeDaemonException)
		{
			for (app_list::iterator it = _apps.begin(); it != _apps.end(); ++it)
			{
				delete (*it);
			}
			_apps.clear();
		}

		void NativeDaemon::init_network() throw (NativeDaemonException)
		{
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
			dtn::core::BundleCore &core = dtn::core::BundleCore::getInstance();

			// get the configuration of the convergence layers
			const std::list<dtn::daemon::Configuration::NetConfig> &nets = conf.getNetwork().getInterfaces();

			// local cl map
			std::map<dtn::daemon::Configuration::NetConfig::NetType, dtn::net::ConvergenceLayer*> _cl_map;

			// holder for file convergence layer
			FileConvergenceLayer *filecl = NULL;

			// add file monitor
#ifdef HAVE_SYS_INOTIFY_H
			FileMonitor *fm = NULL;
#endif

			// create the convergence layers
		 	for (std::list<dtn::daemon::Configuration::NetConfig>::const_iterator iter = nets.begin(); iter != nets.end(); ++iter)
			{
				const dtn::daemon::Configuration::NetConfig &net = (*iter);

				try {
					switch (net.type)
					{
						case dtn::daemon::Configuration::NetConfig::NETWORK_FILE:
						{
							try {
								if (filecl == NULL)
								{
									filecl = new FileConvergenceLayer();
									_components[RUNLEVEL_NETWORK].push_back(filecl);
								}

#ifdef HAVE_SYS_INOTIFY_H
								if (net.url.size() > 0)
								{
									ibrcommon::File path(net.url);

									if (path.exists())
									{
										if (fm == NULL)
										{
											fm = new FileMonitor();
											_components[RUNLEVEL_NETWORK].push_back(fm);
										}
										ibrcommon::File path(net.url);
										fm->watch(path);
									}
								}
#endif
							} catch (const ibrcommon::Exception &ex) {
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add FileConvergenceLayer: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}
							break;
						}

						case dtn::daemon::Configuration::NetConfig::NETWORK_UDP:
						{
							try {
								_components[RUNLEVEL_NETWORK].push_back( new dtn::net::UDPConvergenceLayer( net.iface, net.port, net.mtu ) );
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "UDP ConvergenceLayer added on " << net.iface.toString() << ":" << net.port << IBRCOMMON_LOGGER_ENDL;
							} catch (const ibrcommon::Exception &ex) {
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add UDP ConvergenceLayer on " << net.iface.toString() << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}

							break;
						}

						case dtn::daemon::Configuration::NetConfig::NETWORK_TCP:
						{
							// look for an earlier instance of
							std::map<dtn::daemon::Configuration::NetConfig::NetType, dtn::net::ConvergenceLayer*>::iterator it = _cl_map.find(net.type);

							TCPConvergenceLayer *tcpcl = NULL;

							if (it == _cl_map.end()) {
								tcpcl = new TCPConvergenceLayer();
							} else {
								tcpcl = dynamic_cast<TCPConvergenceLayer*>(it->second);
							}

							try {
								tcpcl->add(net.iface, net.port);

								if (it == _cl_map.end()) {
									_components[RUNLEVEL_NETWORK].push_back(tcpcl);
									_cl_map[net.type] = tcpcl;
								}

								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "TCP ConvergenceLayer added on " << net.iface.toString() << ":" << net.port << IBRCOMMON_LOGGER_ENDL;
							} catch (const ibrcommon::Exception &ex) {
								if (it == _cl_map.end()) {
									delete tcpcl;
								}

								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add TCP ConvergenceLayer on " << net.iface.toString() << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}

							break;
						}

#ifdef HAVE_LIBCURL
						case dtn::daemon::Configuration::NetConfig::NETWORK_HTTP:
						{
							try {
								_components[RUNLEVEL_NETWORK].push_back( new HTTPConvergenceLayer( net.url ) );
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "HTTP ConvergenceLayer added, Server: " << net.url << IBRCOMMON_LOGGER_ENDL;
							} catch (const ibrcommon::Exception &ex) {
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add HTTP ConvergenceLayer, Server: " << net.url << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}
							break;
						}
#endif

#ifdef HAVE_LOWPAN_SUPPORT
						case dtn::daemon::Configuration::NetConfig::NETWORK_LOWPAN:
						{
							try {
								_components[RUNLEVEL_NETWORK].push_back( new LOWPANConvergenceLayer( net.iface, static_cast<uint16_t>(net.port) ) );
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "LOWPAN ConvergenceLayer added on " << net.iface.toString() << ":" << net.port << IBRCOMMON_LOGGER_ENDL;
							} catch (const ibrcommon::Exception &ex) {
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add LOWPAN ConvergenceLayer on " << net.iface.toString() << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}

							break;
						}

						case dtn::daemon::Configuration::NetConfig::NETWORK_DGRAM_LOWPAN:
						{
							try {
								LOWPANDatagramService *lowpan_service = new LOWPANDatagramService( net.iface, static_cast<uint16_t>(net.port) );
								_components[RUNLEVEL_NETWORK].push_back( new DatagramConvergenceLayer(lowpan_service) );
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Datagram ConvergenceLayer (LowPAN) added on " << net.iface.toString() << ":" << net.port << IBRCOMMON_LOGGER_ENDL;
							} catch (const ibrcommon::Exception &ex) {
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add Datagram ConvergenceLayer (LowPAN) on " << net.iface.toString() << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}
							break;
						}
#endif

						case dtn::daemon::Configuration::NetConfig::NETWORK_DGRAM_UDP:
						{
							try {
								UDPDatagramService *dgram_service = new UDPDatagramService( net.iface, net.port, net.mtu );
								_components[RUNLEVEL_NETWORK].push_back( new DatagramConvergenceLayer(dgram_service) );
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Datagram ConvergenceLayer (UDP) added on " << net.iface.toString() << ":" << net.port << IBRCOMMON_LOGGER_ENDL;
							} catch (const ibrcommon::Exception &ex) {
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add Datagram ConvergenceLayer (UDP) on " << net.iface.toString() << ": " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}
							break;
						}

#ifdef HAVE_VMIME
						case dtn::daemon::Configuration::NetConfig::NETWORK_EMAIL:
						{
							try {
								_components[RUNLEVEL_NETWORK].push_back( new EMailConvergenceLayer() );
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "EMail Convergence Layer (MCL) added" << IBRCOMMON_LOGGER_ENDL;
							}catch(const ibrcommon::Exception &ex) {
								IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Failed to add EMail Convergence Layer (MCL): " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							}
							break;
						}
#endif

						default:
							break;
					}
				} catch (const std::exception &ex) {
					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Error: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}

			/****
			 * Init DHT
			 */

#ifdef WITH_DHT_NAMESERVICE
			// create dht naming service if configured
			if (conf.getDHT().enabled()) {
				IBRCOMMON_LOGGER_DEBUG_TAG(NativeDaemon::TAG, 50) << "DHT: is enabled!" << IBRCOMMON_LOGGER_ENDL;
				_components[RUNLEVEL_NETWORK].push_back( new dtn::dht::DHTNameService() );
			}
#endif

#ifdef WITH_WIFIP2P
			if (conf.getP2P().enabled())
			{
				const std::string wifip2p_ctrlpath = conf.getP2P().getCtrlPath();

				// create a wifi-p2p manager
				_components[RUNLEVEL_NETWORK].push_back( new dtn::net::WifiP2PManager(wifip2p_ctrlpath) );
			}
#endif

		 	/***
		 	 * Init Discovery
		 	 */
			if (conf.getDiscovery().enabled())
			{
				// get the discovery port
				int disco_port = conf.getDiscovery().port();

				// collect all interfaces of convergence layer instances
				std::set<ibrcommon::vinterface> interfaces;

				const std::list<dtn::daemon::Configuration::NetConfig> &nets = conf.getNetwork().getInterfaces();
				for (std::list<dtn::daemon::Configuration::NetConfig>::const_iterator iter = nets.begin(); iter != nets.end(); ++iter)
				{
					const dtn::daemon::Configuration::NetConfig &net = (*iter);
					if (!net.iface.empty())
						interfaces.insert(net.iface);
				}

				dtn::net::IPNDAgent *ipnd = new dtn::net::IPNDAgent( disco_port );

				try {
					const std::set<ibrcommon::vaddress> addr = conf.getDiscovery().address();
					for (std::set<ibrcommon::vaddress>::const_iterator iter = addr.begin(); iter != addr.end(); ++iter) {
						ipnd->add(*iter);
					}
				} catch (const dtn::daemon::Configuration::ParameterNotFoundException&) {
					// by default set multicast equivalent of broadcast
					ipnd->add(ibrcommon::vaddress("ff02::142", disco_port, AF_INET6));
					ipnd->add(ibrcommon::vaddress("224.0.0.142", disco_port, AF_INET));
				}

				for (std::set<ibrcommon::vinterface>::const_iterator iter = interfaces.begin(); iter != interfaces.end(); ++iter)
				{
					const ibrcommon::vinterface &i = (*iter);

					// add interfaces to discovery
					ipnd->bind(i);
				}

				/**
				 * register apps at the IPND
				 */
				for (app_list::iterator it = _apps.begin(); it != _apps.end(); ++it)
				{
			 		try {
			 			DiscoveryServiceProvider *dsp = dynamic_cast<DiscoveryServiceProvider*>(*it);
			 			if (dsp != NULL) {
			 				ipnd->addService(dsp);
			 			}
			 		} catch (const std::bad_cast&) { }
				}

				/**
				 * register convergence layers at the IPND
				 */
			 	component_list &clist = _components[RUNLEVEL_NETWORK];
			 	for (component_list::iterator it = clist.begin(); it != clist.end(); ++it)
			 	{
			 		try {
			 			DiscoveryServiceProvider *dsp = dynamic_cast<DiscoveryServiceProvider*>(*it);
			 			if (dsp != NULL) {
			 				ipnd->addService(dsp);
			 			}
			 		} catch (const std::bad_cast&) { }
			 	}

				_components[RUNLEVEL_NETWORK].push_back(ipnd);
			}
			else
			{
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Discovery disabled" << IBRCOMMON_LOGGER_ENDL;
			}

		 	/**
		 	 * register all convergence layers at the bundle core
		 	 */
			component_list &clist = _components[RUNLEVEL_NETWORK];
			for (component_list::iterator it = clist.begin(); it != clist.end(); ++it)
			{
				try {
					ConvergenceLayer *cl = dynamic_cast<ConvergenceLayer*>(*it);
					if (cl != NULL) {
						core.getConnectionManager().add(cl);
					}
				} catch (const std::bad_cast&) { }
			}

			// announce static nodes, create a list of static nodes
			const std::list<Node> &static_nodes = conf.getNetwork().getStaticNodes();

			for (list<Node>::const_iterator iter = static_nodes.begin(); iter != static_nodes.end(); ++iter)
			{
				core.getConnectionManager().add(*iter);
			}
		}

		void NativeDaemon::shutdown_network() throw (NativeDaemonException)
		{
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
			dtn::core::BundleCore &core = dtn::core::BundleCore::getInstance();

			// announce static nodes, create a list of static nodes
			const std::list<Node> &static_nodes = conf.getNetwork().getStaticNodes();

			for (list<Node>::const_iterator iter = static_nodes.begin(); iter != static_nodes.end(); ++iter)
			{
				core.getConnectionManager().remove(*iter);
			}

		 	/**
		 	 * remove all convergence layers at the bundle core
		 	 */
			component_list &clist = _components[RUNLEVEL_NETWORK];
			for (component_list::iterator it = clist.begin(); it != clist.end(); ++it)
			{
				try {
					ConvergenceLayer *cl = dynamic_cast<ConvergenceLayer*>(*it);
					if (cl != NULL) {
						core.getConnectionManager().remove(cl);
					}
				} catch (const std::bad_cast&) { }
			}
		}

		void NativeDaemon::init_routing_extensions() throw (NativeDaemonException)
		{
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();
			dtn::routing::BaseRouter &router = dtn::core::BundleCore::getInstance().getRouter();

			// add static routing extension
			router.add( new dtn::routing::StaticRoutingExtension() );

			// add other routing extensions depending on the configuration
			switch (conf.getNetwork().getRoutingExtension())
			{
			case dtn::daemon::Configuration::FLOOD_ROUTING:
			{
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Using flooding routing extensions" << IBRCOMMON_LOGGER_ENDL;
				router.add( new dtn::routing::FloodRoutingExtension() );

				// add neighbor routing (direct-delivery) extension
				router.add( new dtn::routing::NeighborRoutingExtension() );
				break;
			}

			case dtn::daemon::Configuration::EPIDEMIC_ROUTING:
			{
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Using epidemic routing extensions" << IBRCOMMON_LOGGER_ENDL;
				router.add( new dtn::routing::EpidemicRoutingExtension() );

				// add neighbor routing (direct-delivery) extension
				router.add( new dtn::routing::NeighborRoutingExtension() );
				break;
			}

			case dtn::daemon::Configuration::PROPHET_ROUTING:
			{
				dtn::daemon::Configuration::Network::ProphetConfig prophet_config = conf.getNetwork().getProphetConfig();
				std::string strategy_name = prophet_config.forwarding_strategy;
				dtn::routing::ForwardingStrategy *forwarding_strategy;
				if(strategy_name == "GRTR"){
					forwarding_strategy = new dtn::routing::ProphetRoutingExtension::GRTR_Strategy();
				}
				else if(strategy_name == "GTMX"){
					forwarding_strategy = new dtn::routing::ProphetRoutingExtension::GTMX_Strategy(prophet_config.gtmx_nf_max);
				}
				else{
					IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, error) << "Prophet forwarding strategy " << strategy_name << " not found. Using GRTR as fallback." << IBRCOMMON_LOGGER_ENDL;
					forwarding_strategy = new dtn::routing::ProphetRoutingExtension::GRTR_Strategy();
				}
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Using prophet routing extensions with " << strategy_name << " as forwarding strategy." << IBRCOMMON_LOGGER_ENDL;
				router.add( new dtn::routing::ProphetRoutingExtension(forwarding_strategy, prophet_config.p_encounter_max,
												prophet_config.p_encounter_first, prophet_config.p_first_threshold,
												prophet_config.beta, prophet_config.gamma, prophet_config.delta,
												prophet_config.time_unit, prophet_config.i_typ,
												prophet_config.next_exchange_timeout));

				// add neighbor routing (direct-delivery) extension
				router.add( new dtn::routing::NeighborRoutingExtension() );
				break;
			}

			case dtn::daemon::Configuration::NO_ROUTING:
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Dynamic routing extensions disabled" << IBRCOMMON_LOGGER_ENDL;
				break;

			default:
				IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "Using default routing extensions" << IBRCOMMON_LOGGER_ENDL;

				// add neighbor routing (direct-delivery) extension
				router.add( new dtn::routing::NeighborRoutingExtension() );
				break;
			}

			// initialize all routing extensions
			router.extensionsUp();
		}

		void NativeDaemon::shutdown_routing_extensions() const throw (NativeDaemonException)
		{
			dtn::routing::BaseRouter &router = dtn::core::BundleCore::getInstance().getRouter();

			// disable all routing extensions
			router.extensionsDown();

			// delete all routing extensions
			router.clearExtensions();
		}

		/**
		 * Generate a reload signal
		 */
		void NativeDaemon::reload() throw ()
		{
			// reload logger
			ibrcommon::Logger::reload();

			// send reload signal to all modules
			dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_RELOAD);
		}

		/**
		 * Enable / disable debugging at runtime
		 */
		void NativeDaemon::setDebug(int level) throw ()
		{
			// activate debugging
			ibrcommon::Logger::setVerbosity(level);
			IBRCOMMON_LOGGER_TAG(NativeDaemon::TAG, info) << "debug level set to " << level << IBRCOMMON_LOGGER_ENDL;
		}

		NativeEventLoop::NativeEventLoop(NativeDaemon &daemon)
		 : _daemon(daemon)
		{
		}

		NativeEventLoop::~NativeEventLoop()
		{
		}

		void NativeEventLoop::run(void) throw ()
		{
			dtn::daemon::Configuration &conf = dtn::daemon::Configuration::getInstance();

			// create the event switch object
			dtn::core::EventSwitch &esw = dtn::core::EventSwitch::getInstance();

			// run the event switch loop forever
			if (conf.getDaemon().getThreads() > 1)
			{
				esw.loop( conf.getDaemon().getThreads() );
			}
			else
			{
				esw.loop();
			}

			// terminate event switch component
			esw.terminate();
		}

		void NativeEventLoop::__cancellation() throw ()
		{
			dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_SHUTDOWN);
			dtn::core::EventSwitch::getInstance().shutdown();
		}
	} /* namespace daemon */
} /* namespace dtn */
