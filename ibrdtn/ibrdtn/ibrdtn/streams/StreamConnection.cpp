/*
 * StreamConnection.cpp
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

#include "ibrdtn/streams/StreamConnection.h"
#include "ibrdtn/data/Exceptions.h"
#include "ibrdtn/streams/StreamContactHeader.h"
#include "ibrdtn/streams/StreamDataSegment.h"
#include "ibrdtn/data/Exceptions.h"
#include <ibrcommon/TimeMeasurement.h>
#include <ibrcommon/Logger.h>

using namespace dtn::data;

namespace dtn
{
	namespace streams
	{
		StreamConnection::StreamConnection(StreamConnection::Callback &cb, std::iostream &stream, const dtn::data::Length buffer_size)
		 : std::iostream(&_buf), _callback(cb), _buf(*this, stream, buffer_size), _shutdown_reason(CONNECTION_SHUTDOWN_NOTSET)
		{
		}

		StreamConnection::~StreamConnection()
		{
		}

		void StreamConnection::handshake(const dtn::data::EID &eid, const dtn::data::Timeout &timeout, const dtn::data::Bitset<StreamContactHeader::HEADER_BITS> &flags)
		{
			// create a new header
			dtn::streams::StreamContactHeader header(eid);

			// set timeout
			header._keepalive = static_cast<uint16_t>(timeout);

			// set flags
			header._flags = flags;

			// do the handshake
			_peer = _buf.handshake(header);

			// signal the complete handshake
			_callback.eventConnectionUp(_peer);
		}

		void StreamConnection::reject()
		{
			_buf.reject();
		}

		void StreamConnection::keepalive()
		{
			_buf.keepalive();
		}

		void StreamConnection::shutdown(ConnectionShutdownCases csc)
		{
			if (csc == CONNECTION_SHUTDOWN_SIMPLE_SHUTDOWN)
			{
				// wait for the last ACKs
				_buf.wait();
			}

			// skip if another shutdown is in progress
			{
				ibrcommon::MutexLock l(_shutdown_reason_lock);
				if (_shutdown_reason != CONNECTION_SHUTDOWN_NOTSET)
				{
					_buf.abort();
					return;
				}
				_shutdown_reason = csc;
			}
			
			try {
				switch (csc)
				{
					case CONNECTION_SHUTDOWN_IDLE:
						_buf.shutdown(StreamDataSegment::MSG_SHUTDOWN_IDLE_TIMEOUT);
						_buf.abort();
						_callback.eventTimeout();
						break;
					case CONNECTION_SHUTDOWN_ERROR:
						_buf.abort();
						_callback.eventError();
						break;
					case CONNECTION_SHUTDOWN_SIMPLE_SHUTDOWN:
						_buf.shutdown(StreamDataSegment::MSG_SHUTDOWN_NONE);
						_callback.eventShutdown(csc);
						break;
					case CONNECTION_SHUTDOWN_NODE_TIMEOUT:
						_buf.abort();
						_callback.eventTimeout();
						break;
					case CONNECTION_SHUTDOWN_PEER_SHUTDOWN:
						_buf.shutdown(StreamDataSegment::MSG_SHUTDOWN_NONE);
						_buf.abort();
						_callback.eventShutdown(csc);
						break;
					case CONNECTION_SHUTDOWN_NOTSET:
						_buf.abort();
						_callback.eventShutdown(csc);
						break;
				}
			} catch (const StreamConnection::StreamErrorException&) {
				_callback.eventError();
			}

			_buf.close();
			_callback.eventConnectionDown();
		}

		void StreamConnection::eventShutdown(StreamConnection::ConnectionShutdownCases csc)
		{
			_callback.eventShutdown(csc);
		}

		void StreamConnection::eventBundleAck(const dtn::data::Length &ack)
		{
			_callback.eventBundleAck(ack);
		}

		void StreamConnection::eventBundleRefused()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamConnection", 20) << "bundle has been refused" << IBRCOMMON_LOGGER_ENDL;
			_callback.eventBundleRefused();
		}

		void StreamConnection::eventBundleForwarded()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("StreamConnection", 20) << "bundle has been forwarded" << IBRCOMMON_LOGGER_ENDL;
			_callback.eventBundleForwarded();
		}

		void StreamConnection::connectionTimeout()
		{
			// call superclass
			_callback.eventTimeout();
		}

		void StreamConnection::enableIdleTimeout(const dtn::data::Timeout &seconds)
		{
			_buf.enableIdleTimeout(seconds);
		}
	}
}
