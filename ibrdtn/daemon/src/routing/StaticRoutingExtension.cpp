/*
 * StaticRoutingExtension.cpp
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
#include "routing/StaticRoutingExtension.h"
#include "routing/QueueBundleEvent.h"

#include "net/TransferAbortedEvent.h"
#include "net/TransferCompletedEvent.h"
#include "net/ConnectionEvent.h"
#include "core/EventDispatcher.h"
#include "core/NodeEvent.h"
#include "storage/SimpleBundleStorage.h"

#ifdef HAVE_REGEX_H
#include <routing/StaticRegexRoute.h>
#endif

#include <ibrdtn/utils/Clock.h>

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

#include <typeinfo>
#include <memory>

namespace dtn
{
	namespace routing
	{
		const std::string StaticRoutingExtension::TAG = "StaticRoutingExtension";

		StaticRoutingExtension::StaticRoutingExtension()
		 : next_expire(0)
		{
		}

		StaticRoutingExtension::~StaticRoutingExtension()
		{
			join();

			// delete all static routes
			for (std::list<StaticRoute*>::iterator iter = _routes.begin();
					iter != _routes.end(); ++iter)
			{
				StaticRoute *route = (*iter);
				delete route;
			}
		}

		void StaticRoutingExtension::__cancellation() throw ()
		{
			_taskqueue.abort();
		}

		void StaticRoutingExtension::run() throw ()
		{
			class BundleFilter : public dtn::storage::BundleSelector
			{
			public:
				BundleFilter(const NeighborDatabase::NeighborEntry &entry, const std::list<const StaticRoute*> &routes, const dtn::core::FilterContext &context, const dtn::net::ConnectionManager::protocol_list &plist)
				 : _entry(entry), _routes(routes), _plist(plist), _context(context)
				{};

				virtual ~BundleFilter() {};

				virtual dtn::data::Size limit() const throw () { return _entry.getFreeTransferSlots(); };

				virtual bool addIfSelected(dtn::storage::BundleResult &result, const dtn::data::MetaBundle &meta) const throw (dtn::storage::BundleSelectorException)
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

					// request limits from neighbor database
					try {
						const RoutingLimitations &limits = _entry.getDataset<RoutingLimitations>();

						if (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
						{
							// check if the peer accepts bundles for other nodes
							if (limits.getLimit(RoutingLimitations::LIMIT_LOCAL_ONLY) > 0) return false;
						}
						else
						{
							// check if destination permits non-singleton bundles
							if (limits.getLimit(RoutingLimitations::LIMIT_SINGLETON_ONLY) > 0) return false;
						}

						// check if the payload is too large for the neighbor
						if ((limits.getLimit(RoutingLimitations::LIMIT_FOREIGN_BLOCKSIZE) > 0) &&
							((size_t)limits.getLimit(RoutingLimitations::LIMIT_FOREIGN_BLOCKSIZE) < meta.getPayloadLength())) return false;
					} catch (const NeighborDatabase::DatasetNotAvailableException&) { }

					// do not forward bundles already known by the destination
					if (_entry.has(meta))
					{
						return false;
					}

					// update filter context
					dtn::core::FilterContext context = _context;
					context.setMetaBundle(meta);

					// search for one rule that match
					for (std::list<const StaticRoute*>::const_iterator iter = _routes.begin(); iter != _routes.end(); ++iter)
					{
						const StaticRoute &route = (**iter);

						if (route.match(meta.destination))
						{
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
						}
					}

					return false;
				};

			private:
				const NeighborDatabase::NeighborEntry &_entry;
				const std::list<const StaticRoute*> &_routes;
				const dtn::net::ConnectionManager::protocol_list &_plist;
				const dtn::core::FilterContext &_context;
			};

			// announce static routes here
			const std::multimap<std::string, std::string> &routes = dtn::daemon::Configuration::getInstance().getNetwork().getStaticRoutes();

			for (std::multimap<std::string, std::string>::const_iterator iter = routes.begin(); iter != routes.end(); ++iter)
			{
				const dtn::data::EID nexthop((*iter).second);
				dtn::routing::StaticRouteChangeEvent::raiseEvent(dtn::routing::StaticRouteChangeEvent::ROUTE_ADD, nexthop, (*iter).first);
			}

			RoutingResult list;

			while (true)
			{
				NeighborDatabase &db = (**this).getNeighborDB();
				std::list<const StaticRoute*> routes;

				try {
					Task *t = _taskqueue.poll();
					std::auto_ptr<Task> killer(t);

					IBRCOMMON_LOGGER_DEBUG_TAG(StaticRoutingExtension::TAG, 5) << "processing task " << t->toString() << IBRCOMMON_LOGGER_ENDL;

					try {
						SearchNextBundleTask &task = dynamic_cast<SearchNextBundleTask&>(*t);

						// remove all routes of the previous round
						routes.clear();

						// clear the result list
						list.clear();

						// look for routes to this node
						for (std::list<StaticRoute*>::const_iterator iter = _routes.begin();
								iter != _routes.end(); ++iter)
						{
							const StaticRoute *route = (*iter);
							if (route->getDestination() == task.eid)
							{
								// add to the valid routes
								routes.push_back(route);
							}
						}

						if (!routes.empty())
						{
							// lock the neighbor database while searching for bundles
							{
								// this destination is not handles by any static route
								ibrcommon::MutexLock l(db);
								NeighborDatabase::NeighborEntry &entry = db.get(task.eid, true);

								// check if enough transfer slots available (threshold reached)
								if (!entry.isTransferThresholdReached())
									throw NeighborDatabase::NoMoreTransfersAvailable(task.eid);

								// get a list of protocols supported by both, the local BPA and the remote peer
								const dtn::net::ConnectionManager::protocol_list plist =
										dtn::core::BundleCore::getInstance().getConnectionManager().getSupportedProtocols(entry.eid);

								// create a filter context
								dtn::core::FilterContext context;
								context.setPeer(entry.eid);
								context.setRouting(*this);

								// get the bundle filter of the neighbor
								BundleFilter filter(entry, routes, context, plist);

								// some debug
								IBRCOMMON_LOGGER_DEBUG_TAG(StaticRoutingExtension::TAG, 40) << "search some bundles not known by " << task.eid.getString() << IBRCOMMON_LOGGER_ENDL;

								// query all bundles from the storage
								(**this).getSeeker().get(filter, list);
							}

							// send the bundles as long as we have resources
							for (RoutingResult::const_iterator iter = list.begin(); iter != list.end(); ++iter)
							{
								try {
									// transfer the bundle to the neighbor
									transferTo(task.eid, (*iter).first, (*iter).second);
								} catch (const NeighborDatabase::AlreadyInTransitException&) { };
							}
						}
					} catch (const NeighborDatabase::NoMoreTransfersAvailable &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					} catch (const NeighborDatabase::EntryNotFoundException &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					} catch (const NodeNotAvailableException &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					} catch (const dtn::storage::NoBundleFoundException &ex) {
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 10) << "task " << t->toString() << " aborted: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					} catch (const std::bad_cast&) { };

					try {
						const ProcessBundleTask &task = dynamic_cast<ProcessBundleTask&>(*t);
						IBRCOMMON_LOGGER_DEBUG_TAG(StaticRoutingExtension::TAG, 50) << "search static route for " << task.bundle.toString() << IBRCOMMON_LOGGER_ENDL;

						// check Scope Control Block - do not forward non-group bundles with hop limit <= 1
						if ((task.bundle.hopcount <= 1) && (task.bundle.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))) continue;

						// look for routes to this node
						for (std::list<StaticRoute*>::const_iterator iter = _routes.begin(); iter != _routes.end(); ++iter)
						{
							const StaticRoute &route = (**iter);

							IBRCOMMON_LOGGER_DEBUG_TAG(StaticRoutingExtension::TAG, 50) << "check static route: " << route.toString() << IBRCOMMON_LOGGER_ENDL;

							try {
								if (route.match(task.bundle.destination))
								{
									// lock the neighbor database while checking if the bundle
									// is already known by the peer
									{
										// get data about the potential next-hop
										ibrcommon::MutexLock l(db);
										NeighborDatabase::NeighborEntry &entry = db.get(route.getDestination(), true);

										// do not forward bundles already known by the destination
										if (entry.has(task.bundle)) continue;
									}

									// get a list of protocols supported by both, the local BPA and the remote peer
									const dtn::net::ConnectionManager::protocol_list plist =
											dtn::core::BundleCore::getInstance().getConnectionManager().getSupportedProtocols(route.getDestination());

									// create a filter context
									dtn::core::FilterContext context;
									context.setPeer(route.getDestination());
									context.setRouting(*this);

									// check bundle filter for each possible path
									for (dtn::net::ConnectionManager::protocol_list::const_iterator it = plist.begin(); it != plist.end(); ++it)
									{
										const dtn::core::Node::Protocol &p = (*it);

										// update context with current protocol
										context.setProtocol(p);

										// execute filtering
										dtn::core::BundleFilter::ACTION ret = dtn::core::BundleCore::getInstance().evaluate(dtn::core::BundleFilter::ROUTING, context);

										if (ret == dtn::core::BundleFilter::ACCEPT)
										{
											// transfer the bundle to the neighbor
											transferTo(route.getDestination(), task.bundle, p);
											break;
										}
									}
								}
							} catch (const NeighborDatabase::EntryNotFoundException&) {
								// neighbor is not in the database, can not forward this bundle
							} catch (const NeighborDatabase::NoMoreTransfersAvailable&) {
							} catch (const NeighborDatabase::AlreadyInTransitException&) {
							} catch (const NodeNotAvailableException &ex) {
								// node is not available as neighbor
							};
						}
					} catch (const std::bad_cast&) { };

					try {
						const RouteChangeTask &task = dynamic_cast<RouteChangeTask&>(*t);

						// delete all similar routes
						for (std::list<StaticRoute*>::iterator iter = _routes.begin();
								iter != _routes.end();)
						{
							StaticRoute *route = (*iter);
							if (route->equals(*task.route))
							{
								delete route;
								_routes.erase(iter++);
							}
							else
							{
								++iter;
							}
						}

						if (task.type == RouteChangeTask::ROUTE_ADD)
						{
							_routes.push_back(task.route);
							_taskqueue.push( new SearchNextBundleTask(task.route->getDestination()) );

							if (task.route->getExpiration() > 0)
							{
								ibrcommon::MutexLock l(_expire_lock);
								if (next_expire == 0 || next_expire > task.route->getExpiration())
								{
									next_expire = task.route->getExpiration();
								}
							}
						}
						else
						{
							delete task.route;

							// force a expiration process
							ibrcommon::MutexLock l(_expire_lock);
							next_expire = 1;
						}
					} catch (const std::bad_cast&) { };

					try {
						dynamic_cast<ClearRoutesTask&>(*t);

						// delete all static routes
						for (std::list<StaticRoute*>::iterator iter = _routes.begin();
								iter != _routes.end(); ++iter)
						{
							StaticRoute *route = (*iter);
							delete route;
						}
						_routes.clear();

						ibrcommon::MutexLock l(_expire_lock);
						next_expire = 0;
					} catch (const std::bad_cast&) { };

					try {
						const ExpireTask &task = dynamic_cast<ExpireTask&>(*t);

						ibrcommon::MutexLock l(_expire_lock);
						next_expire = 0;

						// search for expired items
						for (std::list<StaticRoute*>::iterator iter = _routes.begin();
								iter != _routes.end();)
						{
							StaticRoute *route = (*iter);

							if ((route->getExpiration() > 0) && (route->getExpiration() < task.timestamp))
							{
								route->raiseExpired();
								delete route;
								_routes.erase(iter++);
							}
							else
							{
								if ((next_expire == 0) || (next_expire > route->getExpiration()))
								{
									next_expire = route->getExpiration();
								}

								++iter;
							}
						}
					} catch (const std::bad_cast&) { };

				} catch (const std::exception &ex) {
					IBRCOMMON_LOGGER_DEBUG_TAG(StaticRoutingExtension::TAG, 15) << "terminated due to " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					return;
				}

				yield();
			}
		}

		void StaticRoutingExtension::eventDataChanged(const dtn::data::EID &peer) throw ()
		{
			_taskqueue.push( new SearchNextBundleTask(peer) );
		}

		void StaticRoutingExtension::eventBundleQueued(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) throw ()
		{
			_taskqueue.push( new ProcessBundleTask(meta, peer) );
		}

		void StaticRoutingExtension::raiseEvent(const dtn::core::TimeEvent&) throw ()
		{
			// each second, look for expired routes
			const dtn::data::Timestamp monotonic = dtn::utils::Clock::getMonotonicTimestamp();

			ibrcommon::MutexLock l(_expire_lock);
			if ((next_expire != 0) && (next_expire < monotonic))
			{
				_taskqueue.push( new ExpireTask( monotonic ) );
			}
		}

		void StaticRoutingExtension::raiseEvent(const dtn::routing::StaticRouteChangeEvent &route) throw ()
		{
			// on route change, generate a task
			if (route.type == dtn::routing::StaticRouteChangeEvent::ROUTE_CLEAR)
			{
				_taskqueue.push( new ClearRoutesTask() );
				return;
			}

			StaticRoute *r = NULL;

			if (route.pattern.length() > 0)
			{
#ifdef HAVE_REGEX_H
				r = new StaticRegexRoute(route.pattern, route.nexthop);
#else
				dtn::data::Timestamp et = dtn::utils::Clock::getMonotonicTimestamp() + route.timeout;
				r = new EIDRoute(route.pattern, route.nexthop, et);
#endif
			}
			else
			{
				dtn::data::Timestamp et = dtn::utils::Clock::getMonotonicTimestamp() + route.timeout;
				r = new EIDRoute(route.destination, route.nexthop, et);
			}

			switch (route.type)
			{
			case dtn::routing::StaticRouteChangeEvent::ROUTE_ADD:
				_taskqueue.push( new RouteChangeTask( RouteChangeTask::ROUTE_ADD, r ) );
				break;

			case dtn::routing::StaticRouteChangeEvent::ROUTE_DEL:
				_taskqueue.push( new RouteChangeTask( RouteChangeTask::ROUTE_DEL, r ) );
				break;

			default:
				break;
			}
		}

		void StaticRoutingExtension::componentUp() throw ()
		{
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::routing::StaticRouteChangeEvent>::add(this);

			// reset the task queue
			_taskqueue.reset();

			// routine checked for throw() on 15.02.2013
			try {
				// run the thread
				start();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(StaticRoutingExtension::TAG, error) << "componentUp failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void StaticRoutingExtension::componentDown() throw ()
		{
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::routing::StaticRouteChangeEvent>::remove(this);

			// routine checked for throw() on 15.02.2013
			try {
				// stop the thread
				stop();
				join();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG(StaticRoutingExtension::TAG, error) << "componentDown failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		const std::string StaticRoutingExtension::getTag() const throw ()
		{
			return "neighbor";
		}

		StaticRoutingExtension::EIDRoute::EIDRoute(const dtn::data::EID &match, const dtn::data::EID &nexthop, const dtn::data::Timestamp &et)
		 : _nexthop(nexthop), _match(match), expiretime(et)
		{
		}

		StaticRoutingExtension::EIDRoute::~EIDRoute()
		{
		}

		bool StaticRoutingExtension::EIDRoute::match(const dtn::data::EID &eid) const
		{
			return _match.sameHost(eid);
		}

		const dtn::data::EID& StaticRoutingExtension::EIDRoute::getDestination() const
		{
			return _nexthop;
		}

		/**
		 * Describe this route as a one-line-string.
		 * @return
		 */
		const std::string StaticRoutingExtension::EIDRoute::toString() const
		{
			std::stringstream ss;
			ss << _match.getString() << " => " << _nexthop.getString();
			return ss.str();
		}

		const dtn::data::Timestamp& StaticRoutingExtension::EIDRoute::getExpiration() const
		{
			return expiretime;
		}

		void StaticRoutingExtension::EIDRoute::raiseExpired() const
		{
			dtn::routing::StaticRouteChangeEvent::raiseEvent(dtn::routing::StaticRouteChangeEvent::ROUTE_EXPIRED, _nexthop, _match);
		}

		bool StaticRoutingExtension::EIDRoute::equals(const StaticRoute &route) const
		{
			try {
				const StaticRoutingExtension::EIDRoute &r = dynamic_cast<const StaticRoutingExtension::EIDRoute&>(route);
				return (_nexthop == r._nexthop) && (_match == r._match);
			} catch (const std::bad_cast&) {
				return false;
			}
		}

		/****************************************/

		StaticRoutingExtension::SearchNextBundleTask::SearchNextBundleTask(const dtn::data::EID &e)
		 : eid(e)
		{ }

		StaticRoutingExtension::SearchNextBundleTask::~SearchNextBundleTask()
		{ }

		std::string StaticRoutingExtension::SearchNextBundleTask::toString()
		{
			return "SearchNextBundleTask: " + eid.getString();
		}

		/****************************************/

		StaticRoutingExtension::ProcessBundleTask::ProcessBundleTask(const dtn::data::MetaBundle &meta, const dtn::data::EID &o)
		 : bundle(meta), origin(o)
		{ }

		StaticRoutingExtension::ProcessBundleTask::~ProcessBundleTask()
		{ }

		std::string StaticRoutingExtension::ProcessBundleTask::toString()
		{
			return "ProcessBundleTask: " + bundle.toString();
		}

		/****************************************/

		StaticRoutingExtension::ClearRoutesTask::ClearRoutesTask()
		{
		}

		StaticRoutingExtension::ClearRoutesTask::~ClearRoutesTask()
		{
		}

		std::string StaticRoutingExtension::ClearRoutesTask::toString()
		{
			return "ClearRoutesTask";
		}

		/****************************************/

		StaticRoutingExtension::RouteChangeTask::RouteChangeTask(CHANGE_TYPE t, StaticRoute *r)
		 : type(t), route(r)
		{

		}

		StaticRoutingExtension::RouteChangeTask::~RouteChangeTask()
		{

		}

		std::string StaticRoutingExtension::RouteChangeTask::toString()
		{
			return "RouteChangeTask: " + (*route).toString();
		}

		/****************************************/

		StaticRoutingExtension::ExpireTask::ExpireTask(dtn::data::Timestamp t)
		 : timestamp(t)
		{

		}

		StaticRoutingExtension::ExpireTask::~ExpireTask()
		{

		}

		std::string StaticRoutingExtension::ExpireTask::toString()
		{
			std::stringstream ss;
			ss << "ExpireTask: " << timestamp.toString();
			return ss.str();
		}
	}
}
