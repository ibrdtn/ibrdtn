/*
 * EventConnection.cpp
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

#include "EventConnection.h"
#include "core/EventDispatcher.h"

#include <ibrdtn/utils/Utils.h>

namespace dtn
{
	namespace api
	{
		EventConnection::EventConnection(ClientHandler &client, ibrcommon::socketstream &stream)
		 : ProtocolHandler(client, stream), _running(true)
		{
		}

		EventConnection::~EventConnection()
		{
		}

		void EventConnection::raiseEvent(const dtn::core::NodeEvent &node) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			// start with the event tag
			_stream << "Event: " << node.getName() << std::endl;
			_stream << "Action: ";

			switch (node.getAction())
			{
			case dtn::core::NODE_AVAILABLE:
				_stream << "available";
				break;
			case dtn::core::NODE_UNAVAILABLE:
				_stream << "unavailable";
				break;
			case dtn::core::NODE_DATA_ADDED:
				_stream << "data_added";
				break;
			case dtn::core::NODE_DATA_REMOVED:
				_stream << "data_removed";
				break;
			default:
				break;
			}

			_stream << std::endl;

			// write the node eid
			_stream << "EID: " << node.getNode().getEID().getString() << std::endl;

			// close the event
			_stream << std::endl;
		}

		void EventConnection::raiseEvent(const dtn::core::GlobalEvent &global) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			// start with the event tag
			_stream << "Event: " << global.getName() << std::endl;
			_stream << "Action: ";

			switch (global.getAction())
			{
			case dtn::core::GlobalEvent::GLOBAL_BUSY:
				_stream << "busy";
				break;
			case dtn::core::GlobalEvent::GLOBAL_IDLE:
				_stream << "idle";
				break;
			case dtn::core::GlobalEvent::GLOBAL_NORMAL:
				_stream << "normal";
				break;
			case dtn::core::GlobalEvent::GLOBAL_RELOAD:
				_stream << "reload";
				break;
			case dtn::core::GlobalEvent::GLOBAL_SHUTDOWN:
				_stream << "shutdown";
				break;
			case dtn::core::GlobalEvent::GLOBAL_LOW_ENERGY:
				_stream << "low-energy";
				break;
			case dtn::core::GlobalEvent::GLOBAL_INTERNET_AVAILABLE:
				_stream << "internet available";
				break;
			case dtn::core::GlobalEvent::GLOBAL_INTERNET_UNAVAILABLE:
				_stream << "internet unavailable";
				break;
			case dtn::core::GlobalEvent::GLOBAL_START_DISCOVERY:
				_stream << "start discovery";
				break;
			case dtn::core::GlobalEvent::GLOBAL_STOP_DISCOVERY:
				_stream << "stop discovery";
				break;
			default:
				break;
			}
			_stream << std::endl;

			// close the event
			_stream << std::endl;
		}

		void EventConnection::raiseEvent(const dtn::net::BundleReceivedEvent &received) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			// start with the event tag
			_stream << "Event: " << received.getName() << std::endl;
			_stream << "Peer: " << received.peer.getString() << std::endl;
			_stream << "Local: " << (received.fromlocal ? "true" : "false") << std::endl;

			// write the bundle data
			_stream << "Source: " << received.bundle.source.getString() << std::endl;
			_stream << "Timestamp: " << received.bundle.timestamp.toString() << std::endl;
			_stream << "Sequencenumber: " << received.bundle.sequencenumber.toString() << std::endl;
			_stream << "Lifetime: " << received.bundle.lifetime.toString() << std::endl;
			_stream << "Procflags: " << received.bundle.procflags.toString() << std::endl;

			// write the destination eid
			_stream << "Destination: " << received.bundle.destination.getString() << std::endl;

			if (received.bundle.get(dtn::data::PrimaryBlock::FRAGMENT))
			{
				// write fragmentation values
				_stream << "Appdatalength: " << received.bundle.appdatalength.toString() << std::endl;
				_stream << "Fragmentoffset: " << received.bundle.fragmentoffset.toString() << std::endl;
			}

			// close the event
			_stream << std::endl;
		}

		void EventConnection::raiseEvent(const dtn::core::CustodyEvent &custody) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			_stream << "Event: " << custody.getName() << std::endl;
			_stream << "Action: ";

			switch (custody.getAction())
			{
			case dtn::core::CUSTODY_ACCEPT:
				_stream << "accept";
				break;
			case dtn::core::CUSTODY_REJECT:
				_stream << "reject";
				break;
			default:
				break;
			}
			_stream << std::endl;

			// write the bundle data
			_stream << "Source: " << custody.getBundle().source.getString() << std::endl;
			_stream << "Timestamp: " << custody.getBundle().timestamp.toString() << std::endl;
			_stream << "Sequencenumber: " << custody.getBundle().sequencenumber.toString() << std::endl;
			_stream << "Lifetime: " << custody.getBundle().lifetime.toString() << std::endl;
			_stream << "Procflags: " << custody.getBundle().procflags.toString() << std::endl;

			// write the destination eid
			_stream << "Destination: " << custody.getBundle().destination.getString() << std::endl;

			if (custody.getBundle().isFragment())
			{
				// write fragmentation values
				_stream << "Appdatalength: " << custody.getBundle().appdatalength.toString() << std::endl;
				_stream << "Fragmentoffset: " << custody.getBundle().fragmentoffset.toString() << std::endl;
			}

			// close the event
			_stream << std::endl;
		}

		void EventConnection::raiseEvent(const dtn::net::TransferAbortedEvent &aborted) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			// start with the event tag
			_stream << "Event: " << aborted.getName() << std::endl;
			_stream << "Peer: " << aborted.getPeer().getString() << std::endl;

			// write the bundle data
			_stream << "Source: " << aborted.getBundleID().source.getString() << std::endl;
			_stream << "Timestamp: " << aborted.getBundleID().timestamp.toString() << std::endl;
			_stream << "Sequencenumber: " << aborted.getBundleID().sequencenumber.toString() << std::endl;

			if (aborted.getBundleID().isFragment())
			{
				// write fragmentation values
				_stream << "Fragmentoffset: " << aborted.getBundleID().fragmentoffset.toString() << std::endl;
				_stream << "Fragmentpayload: " << aborted.getBundleID().getPayloadLength() << std::endl;
			}

			// close the event
			_stream << std::endl;
		}

		void EventConnection::raiseEvent(const dtn::net::TransferCompletedEvent &completed) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			// start with the event tag
			_stream << "Event: " << completed.getName() << std::endl;
			_stream << "Peer: " << completed.getPeer().getString() << std::endl;

			// write the bundle data
			_stream << "Source: " << completed.getBundle().source.getString() << std::endl;
			_stream << "Timestamp: " << completed.getBundle().timestamp.toString() << std::endl;
			_stream << "Sequencenumber: " << completed.getBundle().sequencenumber.toString() << std::endl;
			_stream << "Lifetime: " << completed.getBundle().lifetime.toString() << std::endl;
			_stream << "Procflags: " << completed.getBundle().procflags.toString() << std::endl;

			// write the destination eid
			_stream << "Destination: " << completed.getBundle().destination.getString() << std::endl;

			if (completed.getBundle().isFragment())
			{
				// write fragmentation values
				_stream << "Appdatalength: " << completed.getBundle().appdatalength.toString() << std::endl;
				_stream << "Fragmentoffset: " << completed.getBundle().fragmentoffset.toString() << std::endl;
			}

			// close the event
			_stream << std::endl;
		}

		void EventConnection::raiseEvent(const dtn::net::ConnectionEvent &connection) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			// start with the event tag
			_stream << "Event: " << connection.getName() << std::endl;
			_stream << "Action: ";

			switch (connection.getState())
			{
			case dtn::net::ConnectionEvent::CONNECTION_UP:
				_stream << "up";
				break;
			case dtn::net::ConnectionEvent::CONNECTION_DOWN:
				_stream << "down";
				break;
			case dtn::net::ConnectionEvent::CONNECTION_SETUP:
				_stream << "setup";
				break;
			case dtn::net::ConnectionEvent::CONNECTION_TIMEOUT:
				_stream << "timeout";
				break;
			default:
				break;
			}
			_stream << std::endl;

			// write the peer eid
			_stream << "Peer: " << connection.getNode().getEID().getString() << std::endl;

			// close the event
			_stream << std::endl;
		}

		void EventConnection::raiseEvent(const dtn::routing::QueueBundleEvent &queued) noexcept
		{
			ibrcommon::MutexLock l(_mutex);
			if (!_running) return;

			// start with the event tag
			_stream << "Event: " << queued.getName() << std::endl;

			// write the bundle data
			_stream << "Source: " << queued.bundle.source.getString() << std::endl;
			_stream << "Timestamp: " << queued.bundle.timestamp.toString() << std::endl;
			_stream << "Sequencenumber: " << queued.bundle.sequencenumber.toString() << std::endl;
			_stream << "Lifetime: " << queued.bundle.lifetime.toString() << std::endl;
			_stream << "Procflags: " << queued.bundle.procflags.toString() << std::endl;

			// write the destination eid
			_stream << "Destination: " << queued.bundle.destination.getString() << std::endl;

			if (queued.bundle.isFragment())
			{
				// write fragmentation values
				_stream << "Appdatalength: " << queued.bundle.appdatalength.toString() << std::endl;
				_stream << "Fragmentoffset: " << queued.bundle.fragmentoffset.toString() << std::endl;
			}

			// close the event
			_stream << std::endl;
		}

		void EventConnection::run()
		{
			std::string buffer;

			// announce protocol change
			_stream << ClientHandler::API_STATUS_OK << " SWITCHED TO EVENT" << std::endl;

			// run as long the stream is ok
			while (_stream.good())
			{
				getline(_stream, buffer);

				// search for '\r\n' and remove the '\r'
				std::string::reverse_iterator iter = buffer.rbegin();
				if ( (*iter) == '\r' ) buffer = buffer.substr(0, buffer.length() - 1);

				std::vector<std::string> cmd = dtn::utils::Utils::tokenize(" ", buffer);
				if (cmd.empty()) continue;

				// return to previous level
				if (cmd[0] == "exit") break;
			}

			ibrcommon::MutexLock l(_mutex);
			_running = false;
		}

		void EventConnection::setup()
		{
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

		void EventConnection::finally()
		{
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

		void EventConnection::__cancellation() noexcept
		{
		}
	} /* namespace api */
} /* namespace dtn */
