/*
 * BundleCore.cpp
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
#include "core/EventDispatcher.h"
#include "core/BundleCore.h"
#include "core/GlobalEvent.h"
#include "core/BundleEvent.h"
#include "core/FragmentManager.h"
#include "routing/QueueBundleEvent.h"
#include "routing/RequeueBundleEvent.h"
#include "routing/StaticRouteChangeEvent.h"

#include <ibrcommon/data/BLOB.h>
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/data/CustodySignalBlock.h>
#include <ibrdtn/data/TrackingBlock.h>
#include <ibrdtn/data/MetaBundle.h>
#include <ibrdtn/data/Exceptions.h>
#include <ibrdtn/data/EID.h>
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/RWLock.h>
#include <ibrcommon/thread/MutexLock.h>

#include "core/filter/LogFilter.h"
#include "core/filter/SecurityFilter.h"

#include "limits.h"
#include <iostream>
#include <typeinfo>
#include <stdint.h>

#include <ibrdtn/ibrdtn.h>

#ifdef IBRDTN_SUPPORT_BSP
#include "security/SecurityManager.h"
#include <ibrdtn/security/PayloadConfidentialBlock.h>
#endif

#ifdef IBRDTN_SUPPORT_COMPRESSION
#include <ibrdtn/data/CompressedPayloadBlock.h>
#endif

using namespace dtn::data;
using namespace dtn::utils;

namespace dtn
{
	namespace core
	{
		const std::string BundleCore::TAG = "BundleCore";
		dtn::data::EID BundleCore::local;

		dtn::data::Length BundleCore::blocksizelimit = 0;
		dtn::data::Length BundleCore::foreign_blocksizelimit = 0;
		dtn::data::Length BundleCore::max_lifetime = 0;
		dtn::data::Length BundleCore::max_timestamp_future = 0;
		dtn::data::Size BundleCore::max_bundles_in_transit = 5;

		bool BundleCore::forwarding = true;
		bool BundleCore::singleton_only = false;

		BundleCore& BundleCore::getInstance()
		{
			static BundleCore instance;
			return instance;
		}

		BundleCore::BundleCore()
		 : _clock(1), _storage(NULL), _seeker(NULL), _router(NULL), _globally_connected(false)
		{
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::BundlePurgeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::net::TransferCompletedEvent>::add(this);
			dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::add(this);
		}

		BundleCore::~BundleCore()
		{
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::BundlePurgeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::net::TransferCompletedEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::remove(this);
		}

		void BundleCore::componentUp() throw ()
		{
			// routine checked for throw() on 15.02.2013
			onConfigurationChanged(dtn::daemon::Configuration::getInstance());

			// initialize connection manager
			_connectionmanager.initialize();

			// initialize wall clock
			_clock.initialize();

			// initialize discovery agent
			_disco_agent.initialize();

			// start a clock
			_clock.startup();

			// start discovery agent
			_disco_agent.startup();
		}

		void BundleCore::componentDown() throw ()
		{
			ibrcommon::LinkManager::getInstance().removeEventListener(this);

			// terminate discovery agent
			_disco_agent.terminate();

			// terminate connection manager
			_connectionmanager.terminate();

			// terminate wall clock
			_clock.terminate();
		}

		void BundleCore::onConfigurationChanged(const dtn::daemon::Configuration &config) throw ()
		{
			// set local eid
			dtn::core::BundleCore::local = config.getNodename();
			IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Local node name: " << dtn::core::BundleCore::local.getString() << IBRCOMMON_LOGGER_ENDL;

			// set block size limit
			dtn::core::BundleCore::blocksizelimit = config.getLimit("blocksize");
			if (dtn::core::BundleCore::blocksizelimit > 0)
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Block size limited to " << dtn::core::BundleCore::blocksizelimit << " bytes" << IBRCOMMON_LOGGER_ENDL;
			}

			// set block size limit
			dtn::core::BundleCore::foreign_blocksizelimit = config.getLimit("foreign_blocksize");
			if (dtn::core::BundleCore::foreign_blocksizelimit > 0)
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Block size of foreign bundles limited to " << dtn::core::BundleCore::foreign_blocksizelimit << " bytes" << IBRCOMMON_LOGGER_ENDL;
			}

			// set the lifetime limit
			dtn::core::BundleCore::max_lifetime = config.getLimit("lifetime");
			if (dtn::core::BundleCore::max_lifetime > 0)
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Lifetime limited to " << dtn::core::BundleCore::max_lifetime << " seconds" << IBRCOMMON_LOGGER_ENDL;
			}

			// set the timestamp limit
			dtn::core::BundleCore::max_timestamp_future = config.getLimit("predated_timestamp");
			if (dtn::core::BundleCore::max_timestamp_future > 0)
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Pre-dated timestamp limited to " << dtn::core::BundleCore::max_timestamp_future << " seconds in the future" << IBRCOMMON_LOGGER_ENDL;
			}

			// set the maximum count of bundles in transit (bundles to send to the CL queue)
			dtn::data::Size transit_limit = config.getLimit("bundles_in_transit");
			if (transit_limit > 0)
			{
				dtn::core::BundleCore::max_bundles_in_transit = transit_limit;
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Limit the number of bundles in transit to " << dtn::core::BundleCore::max_bundles_in_transit << IBRCOMMON_LOGGER_ENDL;
			}

			// enable or disable forwarding of bundles
			if (config.getNetwork().doForwarding())
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Forwarding of bundles enabled." << IBRCOMMON_LOGGER_ENDL;
				BundleCore::forwarding = true;
			}
			else
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Forwarding of bundles disabled." << IBRCOMMON_LOGGER_ENDL;
				BundleCore::forwarding = false;
			}

			// accept or reject non-singleton bundles
			if (config.getNetwork().doAcceptNonSingleton())
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Non-singleton bundles are accepted." << IBRCOMMON_LOGGER_ENDL;
				BundleCore::singleton_only = false;
			}
			else
			{
				IBRCOMMON_LOGGER_TAG(BundleCore::TAG, info) << "Non-singleton bundles are rejected." << IBRCOMMON_LOGGER_ENDL;
				BundleCore::singleton_only = true;
			}

			const std::set<ibrcommon::vinterface> &global_nets = config.getNetwork().getInternetDevices();

			// remove myself from all listeners
			ibrcommon::LinkManager::getInstance().removeEventListener(this);

			// add all listener in the configuration
			for (std::set<ibrcommon::vinterface>::const_iterator iter = global_nets.begin(); iter != global_nets.end(); ++iter)
			{
				ibrcommon::LinkManager::getInstance().addEventListener(*iter, this);
			}

			// check connection state
			check_connection_state();

			// reload all tables
			reload_filter_tables();
		}

		void BundleCore::setStorage(dtn::storage::BundleStorage *storage)
		{
			_storage = storage;
		}

		void BundleCore::setSeeker(dtn::storage::BundleSeeker *seeker)
		{
			_seeker = seeker;
		}

		void BundleCore::setRouter(dtn::routing::BaseRouter *router)
		{
			_router = router;
		}

		dtn::routing::BaseRouter& BundleCore::getRouter() const
		{
			if (_router == NULL)
			{
				throw ibrcommon::Exception("router not set");
			}

			return *_router;
		}

		dtn::storage::BundleStorage& BundleCore::getStorage()
		{
			if (_storage == NULL)
			{
				throw ibrcommon::Exception("No bundle storage is set! Use BundleCore::setStorage() to set a storage.");
			}
			return *_storage;
		}

		dtn::storage::BundleSeeker& BundleCore::getSeeker()
		{
			if (_seeker == NULL)
			{
				throw ibrcommon::Exception("No bundle seeker is set! Use BundleCore::setSeeker() to set a seeker.");
			}
			return *_seeker;
		}

		WallClock& BundleCore::getClock()
		{
			return _clock;
		}

		dtn::net::ConnectionManager& BundleCore::getConnectionManager()
		{
			return _connectionmanager;
		}

		dtn::net::DiscoveryAgent& BundleCore::getDiscoveryAgent()
		{
			return _disco_agent;
		}

		void BundleCore::addRoute(const dtn::data::EID &destination, const dtn::data::EID &nexthop, const dtn::data::Timeout timeout)
		{
			dtn::routing::StaticRouteChangeEvent::raiseEvent(dtn::routing::StaticRouteChangeEvent::ROUTE_ADD, nexthop, destination, timeout);
		}

		void BundleCore::removeRoute(const dtn::data::EID &destination, const dtn::data::EID &nexthop)
		{
			dtn::routing::StaticRouteChangeEvent::raiseEvent(dtn::routing::StaticRouteChangeEvent::ROUTE_DEL, nexthop, destination);
		}

		bool BundleCore::isGloballyConnected() const
		{
			return _globally_connected;
		}

		void BundleCore::raiseEvent(const dtn::routing::QueueBundleEvent &queued) throw ()
		{
			try {
				// get reference to the meta data of the bundle
				const dtn::data::MetaBundle &meta = queued.bundle;

				// ignore fragments
				if (meta.isFragment()) return;

				// if the destination is equal this node...
				if (meta.destination == local)
				{
					// if the delivered variable is still false at the end of this block,
					// we discard the bundle
					bool delivered = false;

					// process this bundle locally
					const dtn::data::Bundle bundle = getStorage().get(meta);

					if (bundle.get(dtn::data::Bundle::APPDATA_IS_ADMRECORD))
					try {
						// check for a custody signal
						const dtn::data::PayloadBlock &payload = bundle.find<dtn::data::PayloadBlock>();

						CustodySignalBlock custody;
						custody.read(payload);

						getStorage().releaseCustody(bundle.source, custody.bundleid);

						IBRCOMMON_LOGGER_DEBUG_TAG("BundleCore", 5) << "custody released for " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

						delivered = true;
					} catch (const AdministrativeBlock::WrongRecordException&) {
						// no custody signal available
					} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
						// no payload block available
					}

					if (delivered)
					{
						// gen a report
						dtn::core::BundleEvent::raise(meta, BUNDLE_DELIVERED);
					}
					else
					{
						// gen a report
						dtn::core::BundleEvent::raise(meta, BUNDLE_DELETED, StatusReportBlock::DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE);
					}

					if (meta.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
					{
						// delete the bundle
						dtn::core::BundlePurgeEvent::raise(meta);
					}
				}
			} catch (const dtn::storage::NoBundleFoundException&) {
			}
		}

		void BundleCore::raiseEvent(const dtn::net::TransferCompletedEvent &completed) throw ()
		{
			const dtn::data::MetaBundle &meta = completed.getBundle();
			const dtn::data::EID &peer = completed.getPeer();

			if ((meta.destination.sameHost(peer))
					&& (meta.procflags & dtn::data::Bundle::DESTINATION_IS_SINGLETON))
			{
				// bundle has been delivered to its destination
				// delete it from our storage
				dtn::core::BundlePurgeEvent::raise(meta);

				IBRCOMMON_LOGGER_TAG("BundleCore", notice) << "singleton bundle delivered: " << meta.toString() << IBRCOMMON_LOGGER_ENDL;

				// gen a report
				dtn::core::BundleEvent::raise(meta, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::NO_ADDITIONAL_INFORMATION);
			}
		}

		void BundleCore::raiseEvent(const dtn::net::TransferAbortedEvent &aborted) throw ()
		{
			if (aborted.reason != dtn::net::TransferAbortedEvent::REASON_REFUSED) return;

			const dtn::data::EID &peer = aborted.getPeer();
			const dtn::data::BundleID &id = aborted.getBundleID();

			try {
				// create meta bundle for futher processing
				const dtn::data::MetaBundle meta = getStorage().info(id);

				if (!(meta.procflags & dtn::data::Bundle::DESTINATION_IS_SINGLETON)) return;

				// if the bundle has been sent by this module delete it
				if (meta.destination.sameHost(peer))
				{
					// bundle is not deliverable
					dtn::core::BundlePurgeEvent::raise(meta, dtn::core::BundlePurgeEvent::NO_ROUTE_KNOWN);
				}
			} catch (const dtn::storage::NoBundleFoundException&) { };
		}

		void BundleCore::raiseEvent(const dtn::core::BundlePurgeEvent &purge) throw ()
		{
			// get the global storage
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

			try {
				// delete the bundle
				storage.remove(purge.bundle);
			} catch (const dtn::storage::NoBundleFoundException&) { };
		}

		void BundleCore::validate(const dtn::data::MetaBundle &obj) const throw (dtn::data::Validator::RejectedException)
		{
			if (dtn::utils::Clock::isExpired(obj)) {
				// ... bundle is expired
				IBRCOMMON_LOGGER_DEBUG_TAG("BundleCore", 35) << "bundle rejected: bundle has expired (" << obj.toString() << ")" << IBRCOMMON_LOGGER_ENDL;
				throw dtn::data::Validator::RejectedException("bundle is expired");
			}

			// check if the lifetime of the bundle is too long
			if (BundleCore::max_lifetime > 0)
			{
				if (obj.lifetime > BundleCore::max_lifetime)
				{
					// ... we reject bundles with such a long lifetime
					IBRCOMMON_LOGGER_DEBUG_TAG("BundleCore", 35) << "lifetime of bundle rejected: " << obj.toString() << IBRCOMMON_LOGGER_ENDL;
					throw dtn::data::Validator::RejectedException("lifetime of the bundle is too long");
				}
			}

			// check if the timestamp is in the future
			if ((BundleCore::max_timestamp_future > 0) && (obj.timestamp != 0))
			{
				// first check if the local clock is reliable
				if (dtn::utils::Clock::getRating() > 0)
					// then check the timestamp
					if ((dtn::utils::Clock::getTime() + BundleCore::max_timestamp_future) < obj.timestamp)
					{
						// ... we reject bundles with a timestamp so far in the future
						IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "timestamp of bundle rejected: " << obj.toString() << IBRCOMMON_LOGGER_ENDL;
						throw dtn::data::Validator::RejectedException("timestamp is too far in the future");
					}
			}

			// use validation filter for further evaluation
			dtn::core::FilterContext context;
			context.setMetaBundle(obj);

			ibrcommon::MutexLock l(_filter_mutex);
			if (_table_validation.evaluate(context) != BundleFilter::ACCEPT)
				throw dtn::data::Validator::RejectedException("rejected by filter");
		}

		void BundleCore::validate(const dtn::data::PrimaryBlock &p) const throw (dtn::data::Validator::RejectedException)
		{
			// check if the bundle is expired
			if (dtn::utils::Clock::isExpired(p.timestamp, p.lifetime))
			{
				IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "bundle rejected: bundle has expired (" << p.toString() << ")" << IBRCOMMON_LOGGER_ENDL;
				throw dtn::data::Validator::RejectedException("bundle is expired");
			}

			// if we do not accept non-singleton bundles
			if (BundleCore::singleton_only && !p.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
			{
				// ... we reject all non-singleton bundles.
				IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "non-singleton bundle rejected: " << p.toString() << IBRCOMMON_LOGGER_ENDL;
				throw dtn::data::Validator::RejectedException("bundle is not addressed to a singleton endpoint");
			}

			// if we do not forward bundles
			if (!BundleCore::forwarding && p.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
			{
				if (!p.destination.sameHost(BundleCore::local))
				{
					// ... we reject all non-local bundles.
					IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "non-local bundle rejected: " << p.toString() << IBRCOMMON_LOGGER_ENDL;
					throw dtn::data::Validator::RejectedException("bundle is not local");
				}
			}

			// check if the lifetime of the bundle is too long
			if (BundleCore::max_lifetime > 0)
			{
				if (p.lifetime > BundleCore::max_lifetime)
				{
					// ... we reject bundles with such a long lifetime
					IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "lifetime of bundle rejected: " << p.toString() << IBRCOMMON_LOGGER_ENDL;
					throw dtn::data::Validator::RejectedException("lifetime of the bundle is too long");
				}
			}

			// check if the timestamp is in the future
			if ((BundleCore::max_timestamp_future > 0) && (p.timestamp != 0))
			{
				// first check if the local clock is reliable
				if (dtn::utils::Clock::getRating() > 0)
					// then check the timestamp
					if ((dtn::utils::Clock::getTime() + BundleCore::max_timestamp_future) < p.timestamp)
					{
						// ... we reject bundles with a timestamp so far in the future
						IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "timestamp of bundle rejected: " << p.toString() << IBRCOMMON_LOGGER_ENDL;
						throw dtn::data::Validator::RejectedException("timestamp is too far in the future");
					}
			}

			// check for duplicates
			if (getRouter().isKnown(p))
			{
				throw dtn::data::Validator::RejectedException("duplicate detected");
			}

			// use validation filter for further evaluation
			dtn::core::FilterContext context;
			context.setPrimaryBlock(p);

			ibrcommon::MutexLock l(_filter_mutex);
			if (_table_validation.evaluate(context) != BundleFilter::ACCEPT)
				throw dtn::data::Validator::RejectedException("rejected by filter");
		}

		void BundleCore::validate(const dtn::data::Block& block, const dtn::data::Number& size) const throw (dtn::data::Validator::RejectedException)
		{
			// check for the size of the block
			// reject a block if it exceeds the payload limit
			if ((BundleCore::blocksizelimit > 0) && (size > BundleCore::blocksizelimit))
			{
				IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "bundle rejected: block size of " << size.toString() << " is too big" << IBRCOMMON_LOGGER_ENDL;
				throw dtn::data::Validator::RejectedException("block size is too big");
			}

			// use validation filter for further evaluation
			dtn::core::FilterContext context;
			context.setBlock(block, size);

			ibrcommon::MutexLock l(_filter_mutex);
			if (_table_validation.evaluate(context) != BundleFilter::ACCEPT)
				throw dtn::data::Validator::RejectedException("rejected by filter");
		}

		void BundleCore::validate(const dtn::data::PrimaryBlock &bundle, const dtn::data::Block& block, const dtn::data::Number& size) const throw (RejectedException)
		{
			// check for the size of the foreign block
			// reject a block if it exceeds the payload limit
			if (BundleCore::foreign_blocksizelimit > 0) {
				if (size > BundleCore::foreign_blocksizelimit) {
					if (!bundle.source.sameHost(dtn::core::BundleCore::local)) {
						if (!bundle.destination.sameHost(dtn::core::BundleCore::local)) {
							IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "foreign bundle " << bundle.toString() << " rejected: block size of " << size.toString() << " is too big" << IBRCOMMON_LOGGER_ENDL;
							throw dtn::data::Validator::RejectedException("foreign block size is too big");
						}
					}
				}
			}

			// check for the size of the block
			// reject a block if it exceeds the payload limit
			if (BundleCore::blocksizelimit > 0)
			{
				if (size > BundleCore::blocksizelimit)
				{
					IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "bundle " << bundle.toString() << " rejected: block size of " << size.toString() << " is too big" << IBRCOMMON_LOGGER_ENDL;
					throw dtn::data::Validator::RejectedException("block size is too big");
				}
			}

			// use validation filter for further evaluation
			dtn::core::FilterContext context;
			context.setPrimaryBlock(bundle);
			context.setBlock(block, size);

			ibrcommon::MutexLock l(_filter_mutex);
			if (_table_validation.evaluate(context) != BundleFilter::ACCEPT)
				throw dtn::data::Validator::RejectedException("rejected by filter");
		}

		void BundleCore::validate(const dtn::data::Bundle &b) const throw (dtn::data::Validator::RejectedException)
		{
			// reject bundles without destination
			if (b.destination.isNone())
			{
				IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "bundle rejected: the destination is null" << IBRCOMMON_LOGGER_ENDL;
				throw dtn::data::Validator::RejectedException("bundle destination is none");
			}

			// check bundle expiration again for bundles with age block
			if ((b.timestamp == 0) && dtn::utils::Clock::isExpired(b))
			{
				IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "bundle rejected: bundle has expired (" << b.toString() << ")" << IBRCOMMON_LOGGER_ENDL;
				throw dtn::data::Validator::RejectedException("bundle is expired");
			}

			// check for invalid blocks
			for (dtn::data::Bundle::const_iterator iter = b.begin(); iter != b.end(); ++iter)
			{
				try {
					const dtn::data::ExtensionBlock &e = dynamic_cast<const dtn::data::ExtensionBlock&>(**iter);

					if (e.get(dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED))
					{
						// reject the hole bundle
						throw dtn::data::Validator::RejectedException("bundle contains unintelligible blocks");
					}

					if (e.get(dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED))
					{
						// transmit status report, because we can not process this block
						dtn::core::BundleEvent::raise(dtn::data::MetaBundle::create(b), BUNDLE_RECEIVED, dtn::data::StatusReportBlock::BLOCK_UNINTELLIGIBLE);
					}
				} catch (const std::bad_cast&) { }
			}

			// use validation filter for further evaluation
			dtn::core::FilterContext context;
			context.setBundle(b);

			ibrcommon::MutexLock l(_filter_mutex);
			if (_table_validation.evaluate(context) != BundleFilter::ACCEPT)
				throw dtn::data::Validator::RejectedException("rejected by filter");
		}

		BundleFilter::ACTION BundleCore::filter(BundleFilter::TABLE table, const FilterContext &context, dtn::data::Bundle &bundle) const
		{
			ibrcommon::MutexLock l(_filter_mutex);

			switch (table)
			{
				case BundleFilter::INPUT:
					return _table_input.filter(context, bundle);

				case BundleFilter::OUTPUT:
					return _table_output.filter(context, bundle);

				case BundleFilter::ROUTING:
					return _table_routing.filter(context, bundle);
			}

			return BundleFilter::ACCEPT;
		}

		BundleFilter::ACTION BundleCore::evaluate(BundleFilter::TABLE table, const FilterContext &context) const
		{
			ibrcommon::MutexLock l(_filter_mutex);

			switch (table)
			{
				case BundleFilter::INPUT:
					return _table_input.evaluate(context);

				case BundleFilter::OUTPUT:
					return _table_output.evaluate(context);

				case BundleFilter::ROUTING:
					return _table_routing.evaluate(context);
			}

			return BundleFilter::ACCEPT;
		}

		const std::string BundleCore::getName() const
		{
			return "BundleCore";
		}

		void BundleCore::processBlocks(dtn::data::Bundle &b)
		{
			bool restart = true;

			// loop as long as necessary over all blocks
			while (restart)
			{
				// disable restart
				restart = false;

				// walk through the block and process them when needed
				for (dtn::data::Bundle::iterator iter = b.begin(); iter != b.end(); ++iter)
				{
					const dtn::data::Block &block = (**iter);

#ifdef IBRDTN_SUPPORT_BSP
					if (block.getType() == dtn::security::PayloadConfidentialBlock::BLOCK_TYPE)
					{
						// try to decrypt the bundle
						try {
							dtn::security::SecurityManager::getInstance().decrypt(b);
						} catch (const dtn::security::SecurityManager::KeyMissingException&) {
							// decrypt needed, but no key is available
							IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "No key available for decryption of bundle." << IBRCOMMON_LOGGER_ENDL;
							throw;
						} catch (const dtn::security::DecryptException &ex) {
							// decrypt failed
							IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "Decryption of bundle failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							throw;
						}

						// bundle has been altered - proceed from the first block
						restart = true;
						break;
					}
#endif

#ifdef IBRDTN_SUPPORT_COMPRESSION
					if (block.getType() == dtn::data::CompressedPayloadBlock::BLOCK_TYPE)
					{
						// try to decompress the bundle
						try {
							dtn::data::CompressedPayloadBlock::extract(b);
						} catch (const ibrcommon::Exception &ex) {
							IBRCOMMON_LOGGER_TAG("BundleCore", warning) << "Decompression of bundle failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
							throw;
						}

						// bundle has been altered - proceed from the first block
						restart = true;
						break;
					}
#endif
				}
			}
		}

		void BundleCore::eventNotify(const ibrcommon::LinkEvent &evt)
		{
			const std::set<ibrcommon::vinterface> &global_nets = dtn::daemon::Configuration::getInstance().getNetwork().getInternetDevices();
			if (global_nets.find(evt.getInterface()) != global_nets.end()) {
				check_connection_state();
			}
		}

		void BundleCore::inject(const dtn::data::EID &source, dtn::data::Bundle &bundle, bool local)
		{
			const dtn::data::MetaBundle m = dtn::data::MetaBundle::create(bundle);

			try {
				if (local)
				{
					IBRCOMMON_LOGGER_TAG(TAG, notice) << "Bundle received " + bundle.toString() + " (local)" << IBRCOMMON_LOGGER_ENDL;

					// create a bundle received event
					dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_RECEIVED);

					// modify TrackingBlock
					try {
						dtn::data::TrackingBlock &track = bundle.find<dtn::data::TrackingBlock>();
						track.append(dtn::core::BundleCore::local);
					} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };

#ifdef IBRDTN_SUPPORT_COMPRESSION
					// if the compression bit is set, then compress the bundle
					if (bundle.get(dtn::data::PrimaryBlock::IBRDTN_REQUEST_COMPRESSION))
					{
						try {
							dtn::data::CompressedPayloadBlock::compress(bundle, dtn::data::CompressedPayloadBlock::COMPRESSION_ZLIB);

							bundle.set(dtn::data::PrimaryBlock::IBRDTN_REQUEST_COMPRESSION, false);
						} catch (const ibrcommon::Exception &ex) {
							IBRCOMMON_LOGGER_TAG(TAG, warning) << "compression of bundle failed: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
						};
					}
#endif

#ifdef IBRDTN_SUPPORT_BSP
					// if the encrypt bit is set, then try to encrypt the bundle
					if (bundle.get(dtn::data::PrimaryBlock::DTNSEC_REQUEST_ENCRYPT))
					{
						try {
							dtn::security::SecurityManager::getInstance().encrypt(bundle);

							bundle.set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_ENCRYPT, false);
						} catch (const dtn::security::SecurityManager::KeyMissingException&) {
							// encryption requested, but no key is available
							IBRCOMMON_LOGGER_TAG(TAG, warning) << "No key available for encrypt process." << IBRCOMMON_LOGGER_ENDL;
						} catch (const dtn::security::EncryptException&) {
							IBRCOMMON_LOGGER_TAG(TAG, warning) << "Encryption of bundle failed." << IBRCOMMON_LOGGER_ENDL;
						}
					}

					// if the sign bit is set, then try to sign the bundle
					if (bundle.get(dtn::data::PrimaryBlock::DTNSEC_REQUEST_SIGN))
					{
						try {
							dtn::security::SecurityManager::getInstance().sign(bundle);

							bundle.set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_SIGN, false);
						} catch (const dtn::security::SecurityManager::KeyMissingException&) {
							// sign requested, but no key is available
							IBRCOMMON_LOGGER_TAG(TAG, warning) << "No key available for sign process." << IBRCOMMON_LOGGER_ENDL;
						}
					}
#endif

					// get the payload size maximum
					const size_t maxPayloadLength = dtn::daemon::Configuration::getInstance().getLimit("payload");

					// check if fragmentation is enabled
					// do not try pro-active fragmentation if the payload length is not limited
					if (dtn::daemon::Configuration::getInstance().getNetwork().doFragmentation() && (maxPayloadLength > 0))
					{
						try {
							std::list<dtn::data::Bundle> fragments;

							dtn::core::FragmentManager::split(bundle, maxPayloadLength, fragments);

							// for each fragment raise bundle received event
							for (std::list<dtn::data::Bundle>::iterator it = fragments.begin(); it != fragments.end(); ++it)
							{
								const dtn::data::MetaBundle m_fragment = dtn::data::MetaBundle::create(*it);

								// store the bundle into a storage module
								getStorage().store(*it);

								// set the bundle as known
								getRouter().setKnown(m_fragment);

								// raise the queued event to notify all receivers about the new bundle
								dtn::routing::QueueBundleEvent::raise(m_fragment, source);
							}

							return;
						} catch (const FragmentationProhibitedException&) {
						} catch (const FragmentationNotNecessaryException&) {
						} catch (const FragmentationAbortedException&) {
							// drop the bundle
							return;
						}
					}

					// store the bundle into a storage module
					getStorage().store(bundle);

					// set the bundle as known
					getRouter().setKnown(m);

					// raise the queued event to notify all receivers about the new bundle
					dtn::routing::QueueBundleEvent::raise(m, source);
				}
				else
				{
					IBRCOMMON_LOGGER_TAG(TAG, notice) << "Bundle received " + bundle.toString() + " from " + source.getString() << IBRCOMMON_LOGGER_ENDL;

					// if the bundle is not known
					if (!getRouter().filterKnown(m))
					{
						// create a bundle received event
						dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_RECEIVED);

						// increment value in the scope control hop limit block
						try {
							dtn::data::ScopeControlHopLimitBlock &schl = bundle.find<dtn::data::ScopeControlHopLimitBlock>();
							schl.increment();
						} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };

						// modify TrackingBlock
						try {
							dtn::data::TrackingBlock &track = bundle.find<dtn::data::TrackingBlock>();
							track.append(dtn::core::BundleCore::local);
						} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };

						// store the bundle into a storage module
						getStorage().store(bundle);

						// raise the queued event to notify all receivers about the new bundle
						dtn::routing::QueueBundleEvent::raise(m, source);
					}
					else
					{
						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 5) << "Duplicate bundle " << bundle.toString() << " from " << source.getString() << " ignored." << IBRCOMMON_LOGGER_ENDL;
					}
				}
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_TAG(TAG, notice) << "Unable to store bundle " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

				// raise BundleEvent because we have to drop the bundle
				dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::DEPLETED_STORAGE);
			} catch (const dtn::storage::BundleStorage::StorageSizeExeededException &ex) {
				IBRCOMMON_LOGGER_TAG(TAG, notice) << "No space left for bundle " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

				// raise BundleEvent because we have to drop the bundle
				dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::DEPLETED_STORAGE);
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(TAG, error) << "Bundle " << bundle.toString() << " dropped: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// raise BundleEvent because we have to drop the bundle
				dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::DEPLETED_STORAGE);
			}
		}

		void BundleCore::setGloballyConnected(bool val)
		{
			if (val == _globally_connected) return;

			if (val) {
				dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_INTERNET_AVAILABLE);
			} else {
				dtn::core::GlobalEvent::raise(dtn::core::GlobalEvent::GLOBAL_INTERNET_UNAVAILABLE);
			}

			_globally_connected = val;
		}

		void BundleCore::check_connection_state() throw ()
		{
			const std::set<ibrcommon::vinterface> &global_nets = dtn::daemon::Configuration::getInstance().getNetwork().getInternetDevices();

			if (!global_nets.empty()) {
				bool found = false;
				for (std::set<ibrcommon::vinterface>::const_iterator iter = global_nets.begin(); iter != global_nets.end(); ++iter)
				{
					const ibrcommon::vinterface &iface = (*iter);
					if (!iface.getAddresses(ibrcommon::vaddress::SCOPE_GLOBAL).empty()) {
						found = true;
					}
				}
				setGloballyConnected(found);
			}
		}

		void BundleCore::reload_filter_tables() throw ()
		{
			ibrcommon::RWLock l(_filter_mutex);

			_table_input.clear();
			_table_output.clear();
			_table_routing.clear();
			_table_validation.clear();

			/**
			 * Add security checks according to the configured security level
			 */
			const dtn::daemon::Configuration::Security &secconf = dtn::daemon::Configuration::getInstance().getSecurity();

			if (secconf.getLevel() & dtn::daemon::Configuration::Security::SECURITY_LEVEL_AUTHENTICATED)
			{
				_table_validation.append(
						(new SecurityFilter(SecurityFilter::VERIFY_AUTH, BundleFilter::SKIP))->append(
						(new LogFilter(ibrcommon::LogLevel::warning, "bundle rejected due to missing authentication"))->append(
						(new RejectFilter())
				)));

				_table_output.append(
						(new SecurityFilter(SecurityFilter::APPLY_AUTH, BundleFilter::SKIP))->append(
						(new LogFilter(ibrcommon::LogLevel::warning, "can not apply authentication due to missing key"))
					));
			}

			if (secconf.getLevel() & dtn::daemon::Configuration::Security::SECURITY_LEVEL_SIGNED)
			{
				_table_validation.append(
						(new SecurityFilter(SecurityFilter::VERIFY_INTEGRITY, BundleFilter::SKIP))->append(
						(new LogFilter(ibrcommon::LogLevel::warning, "bundle rejected due to missing signature"))->append(
						(new RejectFilter())
				)));
			}

			if (secconf.getLevel() & dtn::daemon::Configuration::Security::SECURITY_LEVEL_ENCRYPTED)
			{
				_table_validation.append(
						(new SecurityFilter(SecurityFilter::VERIFY_CONFIDENTIALITY, BundleFilter::SKIP))->append(
						(new LogFilter(ibrcommon::LogLevel::warning, "bundle rejected due to missing encryption"))->append(
						(new RejectFilter())
				)));
			}

			/**
			 * verify integrity and authentication of incoming bundles
			 */
			_table_input.append(
					(new SecurityFilter(SecurityFilter::VERIFY_AUTH, BundleFilter::SKIP))->append(
					(new LogFilter(ibrcommon::LogLevel::warning, "bundle rejected due to invalid authentication"))->append(
					(new RejectFilter())
			)));

			_table_input.append(
					(new SecurityFilter(SecurityFilter::VERIFY_INTEGRITY, BundleFilter::SKIP))->append(
					(new LogFilter(ibrcommon::LogLevel::warning, "bundle rejected due to invalid signature"))->append(
					(new RejectFilter())
			)));
		}
	}
}
