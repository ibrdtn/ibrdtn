/*
 * ProphetRoutingExtension.cpp
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

#include "routing/prophet/ProphetRoutingExtension.h"
#include "routing/prophet/DeliveryPredictabilityMap.h"

#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

#include <algorithm>
#include <memory>
#include <fstream>

#include "core/BundleEvent.h"

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/ThreadsafeReference.h>

#include <ibrdtn/data/SDNV.h>
#include <ibrdtn/data/Exceptions.h>
#include <ibrdtn/utils/Clock.h>

namespace dtn
{
	namespace routing
	{
		const std::string ProphetRoutingExtension::TAG = "ProphetRoutingExtension";

		ProphetRoutingExtension::ProphetRoutingExtension(ForwardingStrategy *strategy, float p_encounter_max, float p_encounter_first, float p_first_threshold,
								 float beta, float gamma, float delta, ibrcommon::Timer::time_t time_unit, ibrcommon::Timer::time_t i_typ,
								 dtn::data::Timestamp next_exchange_timeout)
			: _deliveryPredictabilityMap(time_unit, beta, gamma),
			  _forwardingStrategy(strategy), _next_exchange_timeout(next_exchange_timeout), _next_exchange_timestamp(0),
			  _p_encounter_max(p_encounter_max), _p_encounter_first(p_encounter_first),
			  _p_first_threshold(p_first_threshold), _delta(delta), _i_typ(i_typ)
		{
			// assign myself to the forwarding strategy
			strategy->setProphetRouter(this);

			// set value for local EID to 1.0
			_deliveryPredictabilityMap.set(core::BundleCore::local, 1.0);

			// define the first exchange timestamp
			_next_exchange_timestamp = dtn::utils::Clock::getMonotonicTimestamp() + _next_exchange_timeout;

			try {
				// set file to store prophet data
				ibrcommon::File routing_d = dtn::daemon::Configuration::getInstance().getPath("storage").get("routing");

				// create directory if necessary
				if (!routing_d.isDirectory()) ibrcommon::File::createDirectory(routing_d);

				// assign file within the routing data directory
				_persistent_file = routing_d.get("prophet.dat");
			} catch (const dtn::daemon::Configuration::ParameterNotSetException&) {
				// no path set
			}

			// write something to the syslog
			IBRCOMMON_LOGGER_TAG(ProphetRoutingExtension::TAG, info) << "Initializing PRoPHET routing module" << IBRCOMMON_LOGGER_ENDL;
		}

		ProphetRoutingExtension::~ProphetRoutingExtension()
		{
			stop();
			join();
			delete _forwardingStrategy;
		}

		void ProphetRoutingExtension::requestHandshake(const dtn::data::EID&, NodeHandshake& handshake) const
		{
			handshake.addRequest(DeliveryPredictabilityMap::identifier);
			handshake.addRequest(AcknowledgementSet::identifier);

			// request summary vector to exclude bundles known by the peer
			handshake.addRequest(BloomFilterSummaryVector::identifier);
		}

		void ProphetRoutingExtension::responseHandshake(const dtn::data::EID& neighbor, const NodeHandshake& request, NodeHandshake& response)
		{
			if (request.hasRequest(DeliveryPredictabilityMap::identifier))
			{
				ibrcommon::MutexLock l(_deliveryPredictabilityMap);
				age();
				response.addItem(new DeliveryPredictabilityMap(_deliveryPredictabilityMap));
			}
			if (request.hasRequest(AcknowledgementSet::identifier))
			{
				ibrcommon::MutexLock l(_acknowledgementSet);
				response.addItem(new AcknowledgementSet(_acknowledgementSet));
			}
		}

		void ProphetRoutingExtension::processHandshake(const dtn::data::EID& neighbor, NodeHandshake& response)
		{
			/* ignore neighbors, that have our EID */
			if (neighbor.sameHost(dtn::core::BundleCore::local)) return;

			try {
				const DeliveryPredictabilityMap& neighbor_dp_map = response.get<DeliveryPredictabilityMap>();

				// strip possible application part off the neighbor EID
				const dtn::data::EID neighbor_node = neighbor.getNode();

				IBRCOMMON_LOGGER_DEBUG_TAG(ProphetRoutingExtension::TAG, 10) << "delivery predictability map received from " << neighbor_node.getString() << IBRCOMMON_LOGGER_ENDL;

				// store a copy of the map in the neighbor database
				try {
					NeighborDatabase &db = (**this).getNeighborDB();
					NeighborDataset ds(new DeliveryPredictabilityMap(neighbor_dp_map));

					ibrcommon::MutexLock l(db);
					db.get(neighbor_node).putDataset(ds);
				} catch (const NeighborDatabase::EntryNotFoundException&) { };

				/* update predictability for this neighbor */
				updateNeighbor(neighbor_node, neighbor_dp_map);
			} catch (std::exception&) { }

			try {
				const AcknowledgementSet& neighbor_ack_set = response.get<AcknowledgementSet>();

				IBRCOMMON_LOGGER_DEBUG_TAG(ProphetRoutingExtension::TAG, 10) << "ack'set received from " << neighbor.getString() << IBRCOMMON_LOGGER_ENDL;

				// merge ack'set into the known bundles
				for (AcknowledgementSet::const_iterator it = neighbor_ack_set.begin(); it != neighbor_ack_set.end(); ++it) {
					(**this).setKnown(*it);
				}

				// merge the received ack set with the local one
				{
					ibrcommon::MutexLock l(_acknowledgementSet);
					_acknowledgementSet.merge(neighbor_ack_set);
				}

				/* remove acknowledged bundles from bundle store if we do not have custody */
				dtn::storage::BundleStorage &storage = (**this).getStorage();

				class BundleFilter : public dtn::storage::BundleSelector
				{
				public:
					BundleFilter(const AcknowledgementSet& neighbor_ack_set)
					 : _ackset(neighbor_ack_set)
					{}

					virtual ~BundleFilter() {}

					virtual dtn::data::Size limit() const noexcept { return 0; }

					virtual bool shouldAdd(const dtn::data::MetaBundle &meta) const noexcept (false)
					{
						// do not delete any bundles with
						if (meta.destination.getNode() == dtn::core::BundleCore::local)
							return false;

						if(!_ackset.has(meta))
							return false;

						return true;
					}

				private:
					const AcknowledgementSet& _ackset;
				} filter(neighbor_ack_set);

				dtn::storage::BundleResultList removeList;
				storage.get(filter, removeList);

				for (std::list<dtn::data::MetaBundle>::const_iterator it = removeList.begin(); it != removeList.end(); ++it)
				{
					const dtn::data::MetaBundle &meta = (*it);

					if (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
					{
						dtn::core::BundlePurgeEvent::raise(meta, dtn::core::BundlePurgeEvent::ACK_RECIEVED);
						IBRCOMMON_LOGGER_DEBUG_TAG(ProphetRoutingExtension::TAG, 10) << "Bundle removed due to prophet ack: " << meta.toString() << IBRCOMMON_LOGGER_ENDL;
					}
					else
					{
						IBRCOMMON_LOGGER_TAG(ProphetRoutingExtension::TAG, warning) << neighbor.getString() << " requested to purge a bundle with a non-singleton destination: " << meta.toString() << IBRCOMMON_LOGGER_ENDL;
					}

					/* generate a report */
					dtn::core::BundleEvent::raise(meta, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::NO_ADDITIONAL_INFORMATION);
				}
			} catch (const dtn::storage::NoBundleFoundException&) {
			} catch (std::exception&) { }
		}

		void ProphetRoutingExtension::eventDataChanged(const dtn::data::EID &peer) noexcept
		{
			// transfer the next bundle to this destination
			_taskqueue.push( new SearchNextBundleTask( peer ) );
		}

		void ProphetRoutingExtension::eventTransferCompleted(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) noexcept
		{
			// add forwarded entry to GTMX strategy
			try {
				GTMX_Strategy &gtmx = dynamic_cast<GTMX_Strategy&>(*_forwardingStrategy);
				gtmx.addForward(meta);
			} catch (const std::bad_cast &ex) { };
		}

		void ProphetRoutingExtension::eventBundleQueued(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) noexcept
		{
			// new bundles trigger a recheck for all neighbors
			const std::set<dtn::core::Node> nl = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

			for (std::set<dtn::core::Node>::const_iterator iter = nl.begin(); iter != nl.end(); ++iter)
			{
				const dtn::core::Node &n = (*iter);

				if (n.getEID() != peer)
				{
					// trigger all routing modules to search for bundles to forward
					eventDataChanged(n.getEID());
				}
			}
		}

		void ProphetRoutingExtension::raiseEvent(const dtn::core::TimeEvent &time) noexcept
		{
			// expire bundles in the acknowledgment set
			{
				ibrcommon::MutexLock l(_acknowledgementSet);
				_acknowledgementSet.expire(time.getTimestamp());
			}

			if ((time.getTimestamp().get<size_t>() % 60) == 0)
			{
				// store persistent data to disk
				if (_persistent_file.isValid()) store(_persistent_file);
			}

			ibrcommon::MutexLock l(_next_exchange_mutex);
			const dtn::data::Timestamp now = dtn::utils::Clock::getMonotonicTimestamp();

			if ((_next_exchange_timestamp > 0) && (_next_exchange_timestamp < now))
			{
				_taskqueue.push( new NextExchangeTask() );

				// define the next exchange timestamp
				_next_exchange_timestamp = now + _next_exchange_timeout;
			}
		}

		void ProphetRoutingExtension::raiseEvent(const NodeHandshakeEvent &handshake) noexcept
		{
			if (handshake.state == NodeHandshakeEvent::HANDSHAKE_COMPLETED)
			{
				// transfer the next bundle to this destination
				_taskqueue.push( new SearchNextBundleTask( handshake.peer ) );
			}
		}

		void ProphetRoutingExtension::raiseEvent(const dtn::core::BundlePurgeEvent &purge) noexcept
		{
			if (purge.reason == dtn::core::BundlePurgeEvent::DELIVERED)
			{
				/* the bundle was finally delivered, mark it as acknowledged */
				ibrcommon::MutexLock l(_acknowledgementSet);
				_acknowledgementSet.add(purge.bundle);
			}
		}

		void ProphetRoutingExtension::componentUp() noexcept
		{
			dtn::core::EventDispatcher<dtn::routing::NodeHandshakeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::BundlePurgeEvent>::add(this);

			// reset task queue
			_taskqueue.reset();

			// restore persistent routing data
			if (_persistent_file.exists()) restore(_persistent_file);

			// routine checked for noexcept on 15.02.2013
			try {
				// run the thread
				start();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(ProphetRoutingExtension::TAG, error) << "componentUp failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void ProphetRoutingExtension::componentDown() noexcept
		{
			dtn::core::EventDispatcher<dtn::routing::NodeHandshakeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::BundlePurgeEvent>::remove(this);

			// store persistent routing data
			if (_persistent_file.isValid()) store(_persistent_file);

			try {
				// stop the thread
				stop();
				join();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(ProphetRoutingExtension::TAG, error) << "componentDown failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		const std::string ProphetRoutingExtension::getTag() const noexcept
		{
			return "prophet";
		}

		ibrcommon::ThreadsafeReference<DeliveryPredictabilityMap> ProphetRoutingExtension::getDeliveryPredictabilityMap()
		{
			{
				ibrcommon::MutexLock l(_deliveryPredictabilityMap);
				age();
			}
			return ibrcommon::ThreadsafeReference<DeliveryPredictabilityMap>(_deliveryPredictabilityMap, _deliveryPredictabilityMap);
		}

		ibrcommon::ThreadsafeReference<const DeliveryPredictabilityMap> ProphetRoutingExtension::getDeliveryPredictabilityMap() const
		{
			return ibrcommon::ThreadsafeReference<const DeliveryPredictabilityMap>(_deliveryPredictabilityMap, const_cast<DeliveryPredictabilityMap&>(_deliveryPredictabilityMap));
		}

		ibrcommon::ThreadsafeReference<const AcknowledgementSet> ProphetRoutingExtension::getAcknowledgementSet() const
		{
			return ibrcommon::ThreadsafeReference<const AcknowledgementSet>(_acknowledgementSet, const_cast<AcknowledgementSet&>(_acknowledgementSet));
		}

		void ProphetRoutingExtension::ProphetRoutingExtension::run() noexcept
		{
			class BundleFilter : public dtn::storage::BundleSelector
			{
			public:
				BundleFilter(const NeighborDatabase::NeighborEntry &entry, ForwardingStrategy &strategy, const DeliveryPredictabilityMap &dpm, const std::set<dtn::core::Node> &neighbors, const dtn::core::FilterContext &context, const dtn::net::ConnectionManager::protocol_list &plist)
				 : _entry(entry), _strategy(strategy), _dpm(dpm), _neighbors(neighbors), _plist(plist), _context(context)
				{ };

				virtual ~BundleFilter() {};

				virtual dtn::data::Size limit() const noexcept { return _entry.getFreeTransferSlots(); };

				virtual bool addIfSelected(dtn::storage::BundleResult &result, const dtn::data::MetaBundle &meta) const noexcept (false)
				{
					// check Scope Control Block - do not forward bundles with hop limit == 0
					if (meta.hopcount == 0)
					{
						return false;
					}

					// do not forward local bundles
					if ((meta.destination.getNode() == dtn::core::BundleCore::local)
							&& meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON)
						)
					{
						return false;
					}

					// check Scope Control Block - do not forward non-group bundles with hop limit <= 1
					if ((meta.hopcount <= 1) && (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON)))
					{
						return false;
					}

					// do not forward bundles addressed to this neighbor,
					// because this is handled by neighbor routing extension
					if (_entry.eid == meta.destination.getNode())
					{
						return false;
					}

					// if this is a singleton bundle ...
					if (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
					{
						const dtn::core::Node n(meta.destination.getNode());

						// do not forward the bundle if the final destination is available
						if (_neighbors.find(n) != _neighbors.end())
						{
							return false;
						}

						// check if the neighbor data is up-to-date
						if (!_entry.isFilterValid()) throw dtn::storage::BundleSelectorException();
					}
					else
					{
						// check if the neighbor data is up-to-date
						if (!_entry.isFilterValid()) throw dtn::storage::BundleSelectorException();

						// if this is a non-singleton, check if the peer knows a way to the source
						try {
							if (_dpm.get(meta.source.getNode()) <= 0.0) return false;
						} catch (const dtn::routing::DeliveryPredictabilityMap::ValueNotFoundException&) {
							return false;
						}
					}

					// do not forward bundles already known by the destination
					// throws BloomfilterNotAvailableException if no filter is available or it is expired
					try {
						if (_entry.has(meta, true))
						{
							return false;
						}
					} catch (const dtn::routing::NeighborDatabase::BloomfilterNotAvailableException&) {
						throw dtn::storage::BundleSelectorException();
					}

					// ask the routing strategy if this bundle should be selected
					if (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
					{
						if (!_strategy.shallForward(_dpm, meta)) return false;
					}

					// update filter context
					dtn::core::FilterContext context = _context;
					context.setMetaBundle(meta);

					// check bundle filter for each possible path
					for (dtn::net::ConnectionManager::protocol_list::const_iterator it = _plist.begin(); it != _plist.end(); ++it)
					{
						const dtn::core::Node::Protocol &p = (*it);

						// update context with current protocol
						context.setProtocol(p);

						// execute filtering
						dtn::core::BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().evaluate(dtn::core::BundleFilter::ROUTING, context);

						if (ret == dtn::core::BundleFilter::ACCEPT)
						{
							// put the selected bundle with targeted interface into the result-set
							static_cast<RoutingResult&>(result).put(meta, p);
							return true;
						}
					}

					return false;
				}

			private:
				const NeighborDatabase::NeighborEntry &_entry;
				const ForwardingStrategy &_strategy;
				const DeliveryPredictabilityMap &_dpm;
				const std::set<dtn::core::Node> &_neighbors;
				const dtn::net::ConnectionManager::protocol_list &_plist;
				const dtn::core::FilterContext &_context;
			};

			// list for bundles
			RoutingResult list;

			// set of known neighbors
			std::set<dtn::core::Node> neighbors;

			while (true)
			{
				try {
					Task *t = _taskqueue.poll();
					std::auto_ptr<Task> killer(t);

					IBRCOMMON_LOGGER_DEBUG_TAG(ProphetRoutingExtension::TAG, 50) << "processing task " << t->toString() << IBRCOMMON_LOGGER_ENDL;

					try {
						/**
						 * SearchNextBundleTask triggers a search for a bundle to transfer
						 * to another host. This Task is generated by TransferCompleted, TransferAborted
						 * and node events.
						 */
						try {
							SearchNextBundleTask &task = dynamic_cast<SearchNextBundleTask&>(*t);

							// clear the result list
							list.clear();

							// lock the neighbor database while searching for bundles
							try {
								NeighborDatabase &db = (**this).getNeighborDB();

								ibrcommon::MutexLock l(db);
								NeighborDatabase::NeighborEntry &entry = db.get(task.eid, true);

								// check if enough transfer slots available (threshold reached)
								if (!entry.isTransferThresholdReached())
									throw NeighborDatabase::NoMoreTransfersAvailable();

								// get the DeliveryPredictabilityMap of the potentially next hop
								const DeliveryPredictabilityMap &dpm = entry.getDataset<DeliveryPredictabilityMap>();

								if (dtn::daemon::Configuration::getInstance().getNetwork().doPreferDirect()) {
									// get current neighbor list
									neighbors = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();
								} else {
									// "prefer direct" option disabled - clear the list of neighbors
									neighbors.clear();
								}

								// get a list of protocols supported by both, the local BPA and the remote peer
								const dtn::net::ConnectionManager::protocol_list plist =
										dtn::core::BundleCore::getInstance().getConnectionManager().getSupportedProtocols(entry.eid);

								// create a filter context
								dtn::core::FilterContext context;
								context.setPeer(entry.eid);
								context.setRouting(*this);

								// get the bundle filter of the neighbor
								const BundleFilter filter(entry, *_forwardingStrategy, dpm, neighbors, context, plist);

								// some debug output
								IBRCOMMON_LOGGER_DEBUG_TAG(ProphetRoutingExtension::TAG, 40) << "search some bundles not known by " << task.eid.getString() << IBRCOMMON_LOGGER_ENDL;

								// query some unknown bundle from the storage, the list contains max. 10 items.
								(**this).getSeeker().get(filter, list);
							} catch (const NeighborDatabase::DatasetNotAvailableException&) {
								// if there is no DeliveryPredictabilityMap for the next hop
								// perform a routing handshake with the peer
								(**this).doHandshake(task.eid);
							} catch (const dtn::storage::BundleSelectorException&) {
								// query a new summary vector from this neighbor
								(**this).doHandshake(task.eid);
							}

							// send the bundles as long as we have resources
							for (RoutingResult::const_iterator iter = list.begin(); iter != list.end(); ++iter)
							{
								try {
									transferTo(task.eid, (*iter).first, (*iter).second);
								} catch (const NeighborDatabase::AlreadyInTransitException&) { };
							}
						} catch (const NeighborDatabase::NoMoreTransfersAvailable &ex) {
							IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
						} catch (const NeighborDatabase::EntryNotFoundException &ex) {
							IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
						} catch (const NodeNotAvailableException &ex) {
							IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
						} catch (const dtn::storage::NoBundleFoundException &ex) {
							IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
						} catch (const std::bad_cast&) { }

						/**
						 * NextExchangeTask is a timer based event, that triggers
						 * a new dp_map exchange for every connected node
						 */
						try {
							dynamic_cast<NextExchangeTask&>(*t);

							std::set<dtn::core::Node> neighbors = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();
							std::set<dtn::core::Node>::const_iterator it;
							for(it = neighbors.begin(); it != neighbors.end(); ++it)
							{
								try{
									(**this).doHandshake(it->getEID());
								} catch (const ibrcommon::Exception &ex) { }
							}
						} catch (const std::bad_cast&) { }

					} catch (const ibrcommon::Exception &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(ProphetRoutingExtension::TAG, 20) << "task failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}
				} catch (const std::exception &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG(ProphetRoutingExtension::TAG, 15) << "terminated due to " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					return;
				}

				yield();
			}
		}

		void ProphetRoutingExtension::__cancellation() noexcept
		{
			_taskqueue.abort();
		}

		float ProphetRoutingExtension::p_encounter(const dtn::data::EID &neighbor) const
		{
			age_map::const_iterator it = _ageMap.find(neighbor);
			if(it == _ageMap.end())
			{
				/* In this case, we got a transitive update for the node we have not encountered before */
				return _p_encounter_max;
			}

			const dtn::data::Timestamp currentTime = dtn::utils::Clock::getMonotonicTimestamp();
			const dtn::data::Timestamp time_diff = currentTime - it->second;
#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(currentTime >= it->second && "the ageMap timestamp should be smaller than the current timestamp");
#endif
			if(time_diff > _i_typ)
			{
				return _p_encounter_max;
			}
			else
			{
				return _p_encounter_max * time_diff.get<float>() / static_cast<float>(_i_typ);
			}
		}

		void ProphetRoutingExtension::updateNeighbor(const dtn::data::EID &neighbor, const DeliveryPredictabilityMap& neighbor_dp_map)
		{
			bool shouldPush = false;

			{
				// update the encounter on every routing handshake
				ibrcommon::MutexLock l(_deliveryPredictabilityMap);

				// remember the size of the map before it is altered
				const size_t numOfItems = _deliveryPredictabilityMap.size();

				// age the local predictability map
				age();

				/**
				 * Calculate new value for this encounter
				 */
				try {
					float neighbor_dp = _deliveryPredictabilityMap.get(neighbor);

					if (neighbor_dp < _p_first_threshold)
					{
						neighbor_dp = _p_encounter_first;
					}
					else
					{
						neighbor_dp += (1 - _delta - neighbor_dp) * p_encounter(neighbor);
					}

					_deliveryPredictabilityMap.set(neighbor, neighbor_dp);
				} catch (const DeliveryPredictabilityMap::ValueNotFoundException&) {
					_deliveryPredictabilityMap.set(neighbor, _p_encounter_first);
				}

				_ageMap[neighbor] = dtn::utils::Clock::getMonotonicTimestamp();

				/* update the dp_map */
				_deliveryPredictabilityMap.update(neighbor, neighbor_dp_map, _p_encounter_first);

				// if the number of items has been increased by additional neighbors
				shouldPush = numOfItems < _deliveryPredictabilityMap.size();
			}

			if (shouldPush)
			{
				// then push a notification to all neighbors
				(**this).pushHandshakeUpdated(NodeHandshakeItem::DELIVERY_PREDICTABILITY_MAP);
			}
		}

		void ProphetRoutingExtension::age()
		{
			_deliveryPredictabilityMap.age(_p_first_threshold);
		}

		/**
		 * stores all persistent data to a file
		 */
		void ProphetRoutingExtension::store(const ibrcommon::File &target)
		{
			// open the file
			std::ofstream output(target.getPath().c_str());

			// silently fail
			if (!output.good()) return;

			// lock the predictability map
			ibrcommon::MutexLock l(_deliveryPredictabilityMap);

			// store the predictability map
			_deliveryPredictabilityMap.store(output);

			// store the ack'set
			{
				ibrcommon::MutexLock l(_acknowledgementSet);
				output << _acknowledgementSet;
			}

			// store the number of age-map entries
			output << dtn::data::Number(_ageMap.size());

			// get the current monotonic time-stamp difference
			const dtn::data::Timestamp monotonic_diff = dtn::utils::Clock::getTime() - dtn::utils::Clock::getMonotonicTimestamp();

			// store the age-map
			for (age_map::const_iterator it = _ageMap.begin(); it != _ageMap.end(); ++it)
			{
				const dtn::data::EID &peer = it->first;
				const dtn::data::Timestamp &monotonic_ts = it->second;

				// get a absolute time-stamp
				const dtn::data::Timestamp ts = monotonic_diff + monotonic_ts;

				dtn::data::BundleString peer_entry(peer.getString());

				// write data
				output << peer_entry << ts;
			}
		}

		/**
		 * restore all persistent data from a file
		 */
		void ProphetRoutingExtension::restore(const ibrcommon::File &source)
		{
			// open the file
			std::ifstream input(source.getPath().c_str());

			// silently fail
			if (!input.good()) return;

			// lock the predictability map
			ibrcommon::MutexLock l(_deliveryPredictabilityMap);

			// restore the predictability map
			_deliveryPredictabilityMap.restore(input);

			// restore the ack'set
			{
				ibrcommon::MutexLock l(_acknowledgementSet);
				input >> _acknowledgementSet;
			}

			// clear the age-map
			_ageMap.clear();

			// get the number of age-map entries
			dtn::data::Number num_entries;
			input >> num_entries;

			// get the current monotonic time-stamp difference
			const dtn::data::Timestamp monotonic_diff = dtn::utils::Clock::getTime() - dtn::utils::Clock::getMonotonicTimestamp();
			const dtn::data::Timestamp monotonic_now = dtn::utils::Clock::getMonotonicTimestamp();

			// restore the age-map
			while (input.good() && num_entries > 0)
			{
				dtn::data::BundleString peer_entry;
				dtn::data::Timestamp ts;

				input >> peer_entry >> ts;

				// eliminate time-stamp which are in the future
				if (monotonic_now >= (ts - monotonic_diff))
				{
					// add entry to the map
					_ageMap[dtn::data::EID(peer_entry)] = ts - monotonic_diff;
				}
				else
				{
					// add entry to the map
					_ageMap[dtn::data::EID(peer_entry)] = monotonic_now;
				}

				num_entries--;
			}

			// age the predictability map
			age();
		}

		ProphetRoutingExtension::SearchNextBundleTask::SearchNextBundleTask(const dtn::data::EID &eid)
			: eid(eid)
		{
		}

		ProphetRoutingExtension::SearchNextBundleTask::~SearchNextBundleTask()
		{
		}

		std::string ProphetRoutingExtension::SearchNextBundleTask::toString() const
		{
			return "SearchNextBundleTask: " + eid.getString();
		}

		ProphetRoutingExtension::NextExchangeTask::NextExchangeTask()
		{
		}

		ProphetRoutingExtension::NextExchangeTask::~NextExchangeTask()
		{
		}

		std::string ProphetRoutingExtension::NextExchangeTask::toString() const
		{
			return "NextExchangeTask";
		}

		ProphetRoutingExtension::GRTR_Strategy::GRTR_Strategy()
		{
		}

		ProphetRoutingExtension::GRTR_Strategy::~GRTR_Strategy()
		{
		}

		bool ProphetRoutingExtension::GRTR_Strategy::shallForward(const DeliveryPredictabilityMap& neighbor_dpm, const dtn::data::MetaBundle& bundle) const
		{
			return neighborDPIsGreater(neighbor_dpm, bundle.destination);
		}

		ProphetRoutingExtension::GTMX_Strategy::GTMX_Strategy(unsigned int NF_max)
		 : _NF_max(NF_max)
		{
		}

		ProphetRoutingExtension::GTMX_Strategy::~GTMX_Strategy()
		{
		}

		void ProphetRoutingExtension::GTMX_Strategy::addForward(const dtn::data::BundleID &id)
		{
			nf_map::iterator nf_it = _NF_map.find(id);

			if (nf_it == _NF_map.end()) {
				nf_it = _NF_map.insert(std::make_pair(id, 0)).first;
			}

			++nf_it->second;
		}

		bool ProphetRoutingExtension::GTMX_Strategy::shallForward(const DeliveryPredictabilityMap& neighbor_dpm, const dtn::data::MetaBundle& bundle) const
		{
			unsigned int NF = 0;

			nf_map::const_iterator nf_it = _NF_map.find(bundle);
			if(nf_it != _NF_map.end()) {
				NF = nf_it->second;
			}

			if (NF > _NF_max) return false;

			return neighborDPIsGreater(neighbor_dpm, bundle.destination);
		}

	} // namespace routing
} // namespace dtn
