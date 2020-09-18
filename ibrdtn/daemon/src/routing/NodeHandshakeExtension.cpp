/*
 * NodeHandshakeExtension.cpp
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
#include "routing/NodeHandshakeExtension.h"
#include "routing/NodeHandshakeEvent.h"

#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "core/EventDispatcher.h"

#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/utils/Clock.h>

#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/RWLock.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace routing
	{
		const std::string NodeHandshakeExtension::TAG = "NodeHandshakeExtension";
		const dtn::data::EID NodeHandshakeExtension::BROADCAST_ENDPOINT("dtn://broadcast.dtn/routing");

		NodeHandshakeExtension::NodeHandshakeExtension()
		 : _endpoint(*this)
		{
		}

		NodeHandshakeExtension::~NodeHandshakeExtension()
		{
		}

		void NodeHandshakeExtension::requestHandshake(const dtn::data::EID&, NodeHandshake &request) const
		{
			request.addRequest(BloomFilterPurgeVector::identifier);
		}

		void NodeHandshakeExtension::responseHandshake(const dtn::data::EID&, const NodeHandshake &request, NodeHandshake &answer)
		{
			if (request.hasRequest(BloomFilterSummaryVector::identifier))
			{
				// add own summary vector to the message
				const dtn::data::BundleSet vec = (**this).getKnownBundles();

				// create an item
				BloomFilterSummaryVector *item = new BloomFilterSummaryVector(vec);

				// add it to the handshake
				answer.addItem(item);
			}

			if (request.hasRequest(BloomFilterPurgeVector::identifier))
			{
				// add own purge vector to the message
				const dtn::data::BundleSet vec = (**this).getPurgedBundles();

				// create an item
				BloomFilterPurgeVector *item = new BloomFilterPurgeVector(vec);

				// add it to the handshake
				answer.addItem(item);
			}
		}

		void NodeHandshakeExtension::processHandshake(const dtn::data::EID &source, NodeHandshake &answer)
		{
			try {
				const BloomFilterSummaryVector bfsv = answer.get<BloomFilterSummaryVector>();

				IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 10) << "summary vector received from " << source.getString() << IBRCOMMON_LOGGER_ENDL;

				// get the summary vector (bloomfilter) of this ECM
				const ibrcommon::BloomFilter &filter = bfsv.getVector().getBloomFilter();

				/**
				 * Update the neighbor database with the received filter.
				 * The filter was sent by the owner, so we assign the contained summary vector to
				 * the EID of the sender of this bundle.
				 */
				NeighborDatabase &db = (**this).getNeighborDB();
				ibrcommon::MutexLock l(db);
				db.get(source.getNode()).update(filter, answer.getLifetime());
			} catch (std::exception&) { };

			try {
				const BloomFilterPurgeVector bfpv = answer.get<BloomFilterPurgeVector>();

				IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 10) << "purge vector received from " << source.getString() << IBRCOMMON_LOGGER_ENDL;

				// get the purge vector (bloomfilter) of this ECM
				const ibrcommon::BloomFilter &purge = bfpv.getVector().getBloomFilter();

				// get a reference to the storage
				dtn::storage::BundleStorage &storage = (**this).getStorage();

				// create a bundle filter which selects bundles contained in the received
				// purge vector but not addressed locally
				class BundleFilter : public dtn::storage::BundleSelector
				{
				public:
					BundleFilter(const ibrcommon::BloomFilter &filter)
					 : _filter(filter)
					{};

					virtual ~BundleFilter() {};

					virtual dtn::data::Size limit() const noexcept { return 100; };

					virtual bool shouldAdd(const dtn::data::MetaBundle &meta) const noexcept (false)
					{
						// do not select locally addressed bundles
						if (meta.destination.getNode() == dtn::core::BundleCore::local)
							return false;

						// do not purge non-singleton bundles
						if (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
							return false;

						// select the bundle if it is in the filter
						return meta.isIn(_filter);
					};

					const ibrcommon::BloomFilter &_filter;
				} bundle_filter(purge);

				dtn::storage::BundleResultList list;

				// while we are getting more results from the storage
				do {
					// delete all previous results
					list.clear();

					// query for more bundles
					storage.get(bundle_filter, list);

					for (dtn::storage::BundleResultList::const_iterator iter = list.begin(); iter != list.end(); ++iter)
					{
						const dtn::data::MetaBundle &meta = (*iter);

						// delete bundle from storage
						storage.remove(meta);

						// log the purged bundle
						IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 10) << "bundle purged: " << meta.toString() << IBRCOMMON_LOGGER_ENDL;

						// gen a report
						dtn::core::BundleEvent::raise(meta, dtn::core::BUNDLE_DELETED, StatusReportBlock::NO_ADDITIONAL_INFORMATION);

						// add this bundle to the own purge vector
						(**this).setPurged(meta);
					}
				} while (!list.empty());
			} catch (std::exception&) { };
		}

		void NodeHandshakeExtension::doHandshake(const dtn::data::EID &eid)
		{
			_endpoint.query(eid);
		}

		void NodeHandshakeExtension::pushHandshakeUpdated(const NodeHandshakeItem::IDENTIFIER id)
		{
			_endpoint.push(id);
		}

		void NodeHandshakeExtension::componentUp() noexcept
		{
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::add(this);
		}

		void NodeHandshakeExtension::componentDown() noexcept
		{
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::remove(this);
		}

		void NodeHandshakeExtension::raiseEvent(const dtn::core::NodeEvent &nodeevent) noexcept
		{
			// If a new neighbor comes available, send him a request for the summary vector
			// If a neighbor went away we can free the stored summary vector
			const dtn::core::Node &n = nodeevent.getNode();

			if (nodeevent.getAction() == NODE_UNAVAILABLE)
			{
				// remove the item from the blacklist
				_endpoint.removeFromBlacklist(n.getEID());
			}
		}

		NodeHandshakeExtension::HandshakeEndpoint::HandshakeEndpoint(NodeHandshakeExtension &callback)
		 : _callback(callback)
		{
			AbstractWorker::initialize("routing");
			AbstractWorker::subscribe(BROADCAST_ENDPOINT);
		}

		NodeHandshakeExtension::HandshakeEndpoint::~HandshakeEndpoint()
		{
		}

		void NodeHandshakeExtension::HandshakeEndpoint::callbackBundleReceived(const Bundle &b)
		{
			// do not process bundles send from this endpoint
			if (b.source.getNode() == dtn::core::BundleCore::getInstance().local) return;

			_callback.processHandshake(b);
		}

		void NodeHandshakeExtension::HandshakeEndpoint::send(dtn::data::Bundle &b)
		{
			transmit(b);
		}

		void NodeHandshakeExtension::HandshakeEndpoint::removeFromBlacklist(const dtn::data::EID &eid)
		{
			ibrcommon::MutexLock l(_blacklist_lock);
			_blacklist.erase(eid);
		}

		void NodeHandshakeExtension::HandshakeEndpoint::push(const NodeHandshakeItem::IDENTIFIER id)
		{
			// create a new notification
			NodeHandshake notification(NodeHandshake::HANDSHAKE_NOTIFICATION);
			notification.addRequest(id);

			IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 15) << "handshake notification for type: " << id << IBRCOMMON_LOGGER_ENDL;

			// create a new bundle with a zero timestamp (+age block)
			dtn::data::Bundle req(true);

			// set the source of the bundle
			req.source = getWorkerURI();

			// set the destination of the bundle
			req.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, false);
			req.destination = BROADCAST_ENDPOINT;

			// limit the lifetime to 60 seconds
			req.lifetime = 60;

			// set high priority
			req.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
			req.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

			dtn::data::PayloadBlock &p = req.push_back<PayloadBlock>();
			ibrcommon::BLOB::Reference ref = p.getBLOB();

			// serialize the request into the payload
			{
				ibrcommon::BLOB::iostream ios = ref.iostream();
				(*ios) << notification;
			}

			// add a schl block
			dtn::data::ScopeControlHopLimitBlock &schl = req.push_front<dtn::data::ScopeControlHopLimitBlock>();
			schl.setLimit(1);

			// send the bundle
			transmit(req);
		}

		void NodeHandshakeExtension::HandshakeEndpoint::query(const dtn::data::EID &origin)
		{
			{
				ibrcommon::MutexLock l(_blacklist_lock);
				// only query once each 60 seconds
				if (_blacklist[origin] > dtn::utils::Clock::getMonotonicTimestamp()) return;
				_blacklist[origin] = dtn::utils::Clock::getMonotonicTimestamp() + 60;
			}

			// create a new request for the summary vector of the neighbor
			NodeHandshake request(NodeHandshake::HANDSHAKE_REQUEST);

#ifdef IBRDTN_SUPPORT_COMPRESSION
			// request compressed answer
			request.addRequest(NodeHandshakeItem::REQUEST_COMPRESSED_ANSWER);
#endif

			// walk through all extensions to generate a request
			(*_callback).requestHandshake(origin, request);

			IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 15) << "handshake query from " << origin.getString() << ": " << request.toString() << IBRCOMMON_LOGGER_ENDL;

			// create a new bundle with a zero timestamp (+age block)
			dtn::data::Bundle req(true);

			// set the source of the bundle
			req.source = getWorkerURI();

			// set the destination of the bundle
			req.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);
			req.destination = origin;

			// set destination application
			req.destination.setApplication("routing");

			// limit the lifetime to 60 seconds
			req.lifetime = 60;

			// set high priority
			req.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
			req.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

			dtn::data::PayloadBlock &p = req.push_back<PayloadBlock>();
			ibrcommon::BLOB::Reference ref = p.getBLOB();

			// serialize the request into the payload
			{
				ibrcommon::BLOB::iostream ios = ref.iostream();
				(*ios) << request;
			}

			// add a schl block
			dtn::data::ScopeControlHopLimitBlock &schl = req.push_front<dtn::data::ScopeControlHopLimitBlock>();
			schl.setLimit(1);

			// send the bundle
			transmit(req);
		}

		void NodeHandshakeExtension::processHandshake(const dtn::data::Bundle &bundle)
		{
			// read the ecm
			const dtn::data::PayloadBlock &p = bundle.find<dtn::data::PayloadBlock>();
			ibrcommon::BLOB::Reference ref = p.getBLOB();
			NodeHandshake handshake;

			// locked within this region
			{
				ibrcommon::BLOB::iostream s = ref.iostream();
				(*s) >> handshake;
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 15) << "handshake received from " << bundle.source.getString() << ": " << handshake.toString() << IBRCOMMON_LOGGER_ENDL;

			// if this is a request answer with an summary vector
			if (handshake.getType() == NodeHandshake::HANDSHAKE_REQUEST)
			{
				// create a new request for the summary vector of the neighbor
				NodeHandshake response(NodeHandshake::HANDSHAKE_RESPONSE);

				// lock the extension list during the processing
				(**this).responseHandshake(bundle.source, handshake, response);

				IBRCOMMON_LOGGER_DEBUG_TAG(NodeHandshakeExtension::TAG, 15) << "handshake reply to " << bundle.source.getString() << ": " << response.toString() << IBRCOMMON_LOGGER_ENDL;

				// create a new bundle with a zero timestamp (+age block)
				dtn::data::Bundle answer(true);

				// set the source of the bundle
				answer.source = _endpoint.getWorkerURI();

				// set the destination of the bundle
				answer.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);
				answer.destination = bundle.source;

				// limit the lifetime to 60 seconds
				answer.lifetime = 60;

				// set high priority
				answer.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
				answer.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

				dtn::data::PayloadBlock &p = answer.push_back<PayloadBlock>();
				ibrcommon::BLOB::Reference ref = p.getBLOB();

				// serialize the request into the payload
				{
					ibrcommon::BLOB::iostream ios = ref.iostream();
					(*ios) << response;
				}

				// add a schl block
				dtn::data::ScopeControlHopLimitBlock &schl = answer.push_front<dtn::data::ScopeControlHopLimitBlock>();
				schl.setLimit(1);

				// request compression
				answer.set(dtn::data::PrimaryBlock::IBRDTN_REQUEST_COMPRESSION, true);

				// transfer the bundle to the neighbor
				_endpoint.send(answer);

				// call handshake completed event
				NodeHandshakeEvent::raiseEvent( NodeHandshakeEvent::HANDSHAKE_REPLIED, bundle.source.getNode() );
			}
			else if (handshake.getType() == NodeHandshake::HANDSHAKE_RESPONSE)
			{
				// walk through all extensions to process the contents of the response
				(**this).processHandshake(bundle.source, handshake);

				// call handshake completed event
				NodeHandshakeEvent::raiseEvent( NodeHandshakeEvent::HANDSHAKE_COMPLETED, bundle.source.getNode() );
			}
			else if (handshake.getType() == NodeHandshake::HANDSHAKE_NOTIFICATION)
			{
				// create a new request
				NodeHandshake request(NodeHandshake::HANDSHAKE_REQUEST);

				// walk through all extensions to find out which requests are of interest
				(**this).requestHandshake(BROADCAST_ENDPOINT, request);

				const NodeHandshake::request_set &rs = handshake.getRequests();
				for (NodeHandshake::request_set::const_iterator it = rs.begin(); it != rs.end(); ++it)
				{
					if (request.hasRequest(*it)) {
						// get node endpoint
						const dtn::data::EID node = bundle.source.getNode();

						// clear 60 second blockage
						_endpoint.removeFromBlacklist(node);

						// execute handshake
						_endpoint.query(node);
						return;
					}
				}
			}
		}
	} /* namespace routing */
} /* namespace dtn */
