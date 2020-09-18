/*
 * LOWPANConnection.cpp
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

#include "net/LOWPANConnection.h"
#include "net/BundleReceivedEvent.h"
#include "core/BundleEvent.h"
#include "core/BundleCore.h"

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

#include <ibrdtn/utils/Utils.h>
#include <ibrdtn/data/Serializer.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <list>

using namespace dtn::data;

namespace dtn
{
	namespace net
	{
		LOWPANConnection::LOWPANConnection(const ibrcommon::vaddress &address, LOWPANConvergenceLayer &cl)
			: _address(address), _sender(_stream), _stream(cl, address), _cl(cl)
		{
		}

		LOWPANConnection::~LOWPANConnection()
		{
		}

		ibrcommon::lowpanstream& LOWPANConnection::getStream()
		{
			return _stream;
		}

		void LOWPANConnection::setup() noexcept
		{
			_sender.start();
		}

		void LOWPANConnection::finally() noexcept
		{
			_sender.stop();
			_sender.join();

			// remove this connection from the connection list
			_cl.remove(this);
		}

		void LOWPANConnection::run() noexcept
		{
			try {
				// create a filter context
				dtn::core::FilterContext context;
				context.setProtocol(_cl.getDiscoveryProtocol());

				while(_stream.good())
				{
					try {
						dtn::data::DefaultDeserializer deserializer(_stream, dtn::core::BundleCore::getInstance());
						dtn::data::Bundle bundle;
						deserializer >> bundle;

						IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnection", 10) << "LOWPANConnection::run"<< IBRCOMMON_LOGGER_ENDL;

						// determine sender
						std::stringstream ss; ss << "lowpan:" << _address.address() << "." << _address.service();
						EID sender(ss.str());

						// push bundle through the filter routines
						context.setBundle(bundle);
						context.setPeer(sender);
						BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::INPUT, context, bundle);

						if (ret == BundleFilter::ACCEPT) {
							// raise default bundle received event
							dtn::net::BundleReceivedEvent::raise(sender, bundle, false);
						}
					} catch (const dtn::InvalidDataException &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnection", 10) << "Received a invalid bundle: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					} catch (const ibrcommon::IOException &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnection", 10) << "IOException: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}
			} catch (std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnection", 10) << "Thread died: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void LOWPANConnection::__cancellation() noexcept
		{
			_sender.stop();
		}

		// class LOWPANConnectionSender
		LOWPANConnectionSender::LOWPANConnectionSender(ibrcommon::lowpanstream &stream)
		: _stream(stream)
		{
		}

		LOWPANConnectionSender::~LOWPANConnectionSender()
		{
		}

		void LOWPANConnectionSender::queue(const dtn::net::BundleTransfer &job)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnectionSender", 85) << "queue"<< IBRCOMMON_LOGGER_ENDL;
			_queue.push(job);
		}

		void LOWPANConnectionSender::run() noexcept
		{
			try {
				// create a filter context
				dtn::core::FilterContext context;
				context.setProtocol(dtn::core::Node::CONN_LOWPAN);

				while(_stream.good())
				{
					dtn::net::BundleTransfer job = _queue.poll();
					dtn::data::DefaultSerializer serializer(_stream);

					IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnectionSender", 85) << "run"<< IBRCOMMON_LOGGER_ENDL;

					dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

					// read the bundle out of the storage
					dtn::data::Bundle bundle = storage.get(job.getBundle());

					// push bundle through the filter routines
					context.setPeer(job.getNeighbor());
					context.setBundle(bundle);
					BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().filter(dtn::core::BundleFilter::OUTPUT, context, bundle);

					if (ret != BundleFilter::ACCEPT) {
						job.abort(dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER);
						continue;
					}

					// Put bundle into stringstream
					serializer << bundle; _stream.flush();
					// raise bundle event
					job.complete();
				}
				// FIXME: Exit strategy when sending on socket failed. Like destroying the connection object
				// Also check what needs to be done when the node is not reachable (transfer requeue...)

				IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnectionSender", 45) << "stream destroyed"<< IBRCOMMON_LOGGER_ENDL;
			} catch (std::exception &ex) {
				IBRCOMMON_LOGGER_DEBUG_TAG("LOWPANConnectionSender", 40) << "Thread died: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void LOWPANConnectionSender::__cancellation() noexcept
		{
			_queue.abort();
		}
	}
}
