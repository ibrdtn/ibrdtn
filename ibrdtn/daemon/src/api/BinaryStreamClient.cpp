/*
 * BinaryStreamClient.cpp
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
#include "api/BinaryStreamClient.h"
#include "core/GlobalEvent.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include <ibrdtn/streams/StreamContactHeader.h>
#include <ibrdtn/data/Serializer.h>
#include <iostream>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace api
	{
		BinaryStreamClient::BinaryStreamClient(ClientHandler &client, ibrcommon::socketstream &stream)
		 : ProtocolHandler(client, stream), _sender(*this), _connection(*this, _stream), _lastack(0)
		{
		}

		BinaryStreamClient::~BinaryStreamClient()
		{
			_client.getRegistration().abort();
			_sender.join();
		}

		const dtn::data::EID& BinaryStreamClient::getPeer() const
		{
			return _eid;
		}

		void BinaryStreamClient::eventShutdown(dtn::streams::StreamConnection::ConnectionShutdownCases) noexcept
		{
		}

		void BinaryStreamClient::eventTimeout() noexcept
		{
		}

		void BinaryStreamClient::eventError() noexcept
		{
		}

		void BinaryStreamClient::eventConnectionUp(const dtn::streams::StreamContactHeader &header) noexcept
		{
			Registration &reg = _client.getRegistration();

			if (header._localeid.isNone())
			{
				// create an EID based on the registration handle
				_eid = reg.getDefaultEID();
			}
			else
			{
				// contact received event
				_eid = BundleCore::local;
				_eid.setApplication( header._localeid.getSSP() );
			}

			IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 20) << "new client connected, handle: " << reg.getHandle() << "; eid: "<< _eid.getString() << IBRCOMMON_LOGGER_ENDL;

			reg.subscribe(_eid);
		}

		void BinaryStreamClient::eventConnectionDown() noexcept
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 40) << "BinaryStreamClient::eventConnectionDown()" << IBRCOMMON_LOGGER_ENDL;

			_client.getRegistration().unsubscribe(_eid);

			try {
				// stop the sender
				_sender.stop();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG("BinaryStreamClient", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void BinaryStreamClient::eventBundleRefused() noexcept
		{
			try {
				const dtn::data::Bundle bundle = _sentqueue.take();

				// set ACK to zero
				_lastack = 0;

			} catch (const ibrcommon::QueueUnblockedException&) {
				// pop on empty queue!
			}
		}

		void BinaryStreamClient::eventBundleForwarded() noexcept
		{
			try {
				const dtn::data::Bundle bundle = _sentqueue.take();
				const dtn::data::MetaBundle meta = dtn::data::MetaBundle::create(bundle);

				// notify bundle as delivered
				_client.getRegistration().delivered(meta);

				// set ACK to zero
				_lastack = 0;
			} catch (const ibrcommon::QueueUnblockedException&) {
				// pop on empty queue!
			}
		}

		void BinaryStreamClient::eventBundleAck(const dtn::data::Length &ack) noexcept
		{
			_lastack = ack;
		}

		void BinaryStreamClient::__cancellation() noexcept
		{
			// shutdown
			_connection.shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);

			// close the stream
			_stream.close();
		}

		void BinaryStreamClient::finally()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 60) << "BinaryStreamClient down" << IBRCOMMON_LOGGER_ENDL;

			// abort blocking registrations
			_client.getRegistration().abort();

			// close the stream
			_stream.close();

			try {
				// shutdown the sender thread
				_sender.stop();
			} catch (const std::exception&) { };
		}

		void BinaryStreamClient::run()
		{
			try {
				char flags = 0;

				// request acknowledgements
				flags |= dtn::streams::StreamContactHeader::REQUEST_ACKNOWLEDGMENTS;

				// do the handshake
				_connection.handshake(dtn::core::BundleCore::local, 10, flags);

				// start the sender thread
				_sender.start();

				while (_connection.good())
				{
					dtn::data::Bundle bundle;
					dtn::data::DefaultDeserializer(_connection) >> bundle;

					// process the new bundle
					dtn::api::Registration::processIncomingBundle(_eid, bundle);
				}
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG("BinaryStreamClient", error) << "failed to start thread: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_connection.shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
			} catch (const dtn::SerializationFailedException &ex) {
				IBRCOMMON_LOGGER_TAG("BinaryStreamClient", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_connection.shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_connection.shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
			} catch (const dtn::InvalidDataException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_connection.shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
			} catch (const std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				_connection.shutdown(dtn::streams::StreamConnection::CONNECTION_SHUTDOWN_ERROR);
			}
		}

		bool BinaryStreamClient::good() const
		{
			return _stream.good();
		}

		BinaryStreamClient::Sender::Sender(BinaryStreamClient &client)
		 : _client(client)
		{
		}

		BinaryStreamClient::Sender::~Sender()
		{
			ibrcommon::JoinableThread::join();
		}

		void BinaryStreamClient::Sender::__cancellation() noexcept
		{
			// cancel the main thread in here
			this->abort();

			// abort all blocking calls on the registration object
			_client._client.getRegistration().abort();
		}

		void BinaryStreamClient::Sender::run() noexcept
		{
			Registration &reg = _client._client.getRegistration();

			try {
				while (_client.good())
				{
					try {
						dtn::data::Bundle bundle = reg.receive();

						try {
							// process the bundle block (security, compression, ...)
							dtn::core::BundleCore::processBlocks(bundle);
						} catch (const ibrcommon::Exception&) {
							// bundle processing failure
							continue;
						}

						// add bundle to the queue
						_client._sentqueue.push(bundle);

						// transmit the bundle
						dtn::data::DefaultSerializer(_client._connection) << bundle;

						// mark the end of the bundle
						_client._connection << std::flush;
					} catch (const dtn::storage::NoBundleFoundException&) {
						reg.wait_for_bundle();
					}

					// idle a little bit
					yield();
				}
			} catch (const ibrcommon::QueueUnblockedException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 40) << ex.what() << IBRCOMMON_LOGGER_ENDL;
				return;
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			} catch (const dtn::InvalidDataException &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 10) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			} catch (const std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("BinaryStreamClient", 10) << "unexpected API error! " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void BinaryStreamClient::queue(const dtn::data::Bundle &bundle)
		{
			_sender.push(bundle);
		}
	}
}
