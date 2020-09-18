/*
 * Client.cpp
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

#include "ibrdtn/api/Client.h"
#include "ibrdtn/data/SDNV.h"
#include "ibrdtn/data/Exceptions.h"
#include "ibrdtn/streams/StreamDataSegment.h"
#include "ibrdtn/streams/StreamContactHeader.h"

#include <ibrcommon/net/socketstream.h>
#include <ibrcommon/Logger.h>

#include <iostream>
#include <string>

namespace dtn
{
	namespace api
	{
		Client::AsyncReceiver::AsyncReceiver(Client &client)
		 : _client(client), _running(true)
		{
		}

		Client::AsyncReceiver::~AsyncReceiver()
		{
		}

		void Client::AsyncReceiver::__cancellation() noexcept
		{
			_running = false;
		}

		void Client::AsyncReceiver::run() noexcept
		{
			try {
				while (!_client.eof() && _running)
				{
					dtn::data::Bundle b;

					// To receive a bundle, we construct a default deserializer. Such a deserializer
					// convert a byte stream into a bundle object. If this deserialization fails
					// an exception will be thrown.
					dtn::data::DefaultDeserializer(_client) >> b;

					_client.received(b);
					yield();
				}
			} catch (const dtn::api::ConnectionException &ex) {
				IBRCOMMON_LOGGER_TAG("Client::AsyncReceiver", error) << "ConnectionException: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_client.shutdown(CONNECTION_SHUTDOWN_ERROR);
			} catch (const dtn::streams::StreamConnection::StreamErrorException &ex) {
				IBRCOMMON_LOGGER_TAG("Client::AsyncReceiver", error) << "StreamErrorException: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_client.shutdown(CONNECTION_SHUTDOWN_ERROR);
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_TAG("Client::AsyncReceiver", error) << "IOException: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_client.shutdown(CONNECTION_SHUTDOWN_ERROR);
			} catch (const dtn::InvalidDataException &ex) {
				if (_running) {
					IBRCOMMON_LOGGER_TAG("Client::AsyncReceiver", error) << "InvalidDataException: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					_client.shutdown(CONNECTION_SHUTDOWN_ERROR);
				}
			} catch (const std::exception &ex) {
				IBRCOMMON_LOGGER_TAG("Client::AsyncReceiver", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_client.shutdown(CONNECTION_SHUTDOWN_ERROR);
			}
		}

		Client::Client(const std::string &app, const dtn::data::EID &group, ibrcommon::socketstream &stream, const COMMUNICATION_MODE mode)
		  : StreamConnection(*this, stream), lastack(0), _stream(stream), _mode(mode), _app(app), _group(group), _receiver(*this)
		{
		}

		Client::Client(const std::string &app, ibrcommon::socketstream &stream, const COMMUNICATION_MODE mode)
		  : StreamConnection(*this, stream), lastack(0), _stream(stream), _mode(mode), _app(app), _group(), _receiver(*this)
		{
		}

		Client::~Client()
		{
			try {
				// stop the receiver
				_receiver.stop();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("Client", 20) << "ThreadException in Client destructor: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
			
			// Close the stream. This releases all reading or writing threads.
			_stream.close();

			// wait until the async thread has been finished
			_receiver.join();
		}

		void Client::connect()
		{
			// do a handshake
			EID localeid;
			if (_app.length() > 0) localeid = EID("api:" + _app);

			// connection flags
			dtn::data::Bitset<dtn::streams::StreamContactHeader::HEADER_BITS> flags;

			// request acknowledgements
			flags.setBit(dtn::streams::StreamContactHeader::REQUEST_ACKNOWLEDGMENTS, true);

			// set comm. mode
			if (_mode == MODE_SENDONLY) flags.setBit(dtn::streams::StreamContactHeader::HANDSHAKE_SENDONLY, true);

			// receive API banner
			std::string buffer;
			std::getline(_stream, buffer);

			// if requested...
			if (!_group.isNone())
			{
				// join the group
				_stream << "registration add " << _group.getString() << std::endl;

				// read the reply
				std::getline(_stream, buffer);
			}

			// switch to API tcpcl mode
			_stream << "protocol tcpcl" << std::endl;

			// do the handshake (no timeout, no keepalive)
			handshake(localeid, 0, flags);

			try {
				// run the receiver
				_receiver.start();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG("Client", error) << "failed to start Client::Receiver\n" << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void Client::close()
		{
			// shutdown the bundle stream connection
			shutdown(StreamConnection::CONNECTION_SHUTDOWN_SIMPLE_SHUTDOWN);
		}

		void Client::abort()
		{
			_inqueue.abort();

			// shutdown the bundle stream connection
			shutdown(StreamConnection::CONNECTION_SHUTDOWN_ERROR);
		}

		void Client::eventConnectionDown() noexcept
		{
			_inqueue.abort();

			try {
				_receiver.stop();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG("Client", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void Client::eventBundleAck(const dtn::data::Length &ack) noexcept
		{
			lastack = ack;
		}

		void Client::received(const dtn::data::Bundle &b)
		{
			// if we are in send only mode...
			if (_mode != dtn::api::Client::MODE_SENDONLY)
			{
				_inqueue.push(b);
			}

			// ... then discard the received bundle
		}

		void Client::operator<<(const dtn::data::Bundle &b)
		{
			// To send a bundle, we construct a default serializer. Such a serializer convert
			// the bundle data to the standardized form as byte stream.
			dtn::data::DefaultSerializer(*this) << b;

			// Since this method is used to serialize bundles into an StreamConnection, we need to call
			// a flush on the StreamConnection. This signals the stream to set the bundle end flag on
			// the last segment of streaming.
			flush();
		}

		dtn::data::Bundle Client::getBundle(const dtn::data::Timeout timeout) noexcept (false)
		{
			try {
				return _inqueue.poll(timeout * 1000);
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				if (ex.reason == ibrcommon::QueueUnblockedException::QUEUE_TIMEOUT)
				{
					throw ConnectionTimeoutException();
				}
				else if (ex.reason == ibrcommon::QueueUnblockedException::QUEUE_ABORT)
				{
					throw ConnectionAbortedException(ex.what());
				}

				throw ConnectionException(ex.what());
			} catch (const std::exception &ex) {
				throw ConnectionException(ex.what());
			}

			throw ConnectionException();
		}
	}
}
