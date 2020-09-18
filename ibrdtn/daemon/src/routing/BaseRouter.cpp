/*
 * BaseRouter.cpp
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

#include "routing/BaseRouter.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"
#include "storage/BundleStorage.h"
#include "core/BundleEvent.h"

#include <ibrdtn/data/TrackingBlock.h>
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/utils/Clock.h>

#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/thread/RWLock.h>

#include <ibrdtn/ibrdtn.h>
#ifdef IBRDTN_SUPPORT_BSP
#include "security/SecurityManager.h"
#endif

namespace dtn
{
	namespace routing
	{
		const std::string BaseRouter::TAG = "BaseRouter";

		/**
		 * implementation of the BaseRouter class
		 */
		BaseRouter::BaseRouter()
		 : _known_bundles("router-known-bundles"), _purged_bundles("router-purged-bundles"), _extension_state(false), _next_expiration(0)
		{
			// make the router globally available
			dtn::core::BundleCore::getInstance().setRouter(this);
		}

		BaseRouter::~BaseRouter()
		{
			// unregister this router from the core
			dtn::core::BundleCore::getInstance().setRouter(NULL);

			// delete all extensions
			clearExtensions();
		}

		/**
		 * Add a routing extension to the routing core.
		 * @param extension
		 */
		void BaseRouter::add(RoutingExtension *extension)
		{
			ibrcommon::RWLock l(_extensions_mutex);
			_extensions.insert(extension);
		}

		void BaseRouter::remove(RoutingExtension *extension)
		{
			ibrcommon::RWLock l(_extensions_mutex);
			_extensions.erase(extension);
		}

		ibrcommon::RWMutex& BaseRouter::getExtensionMutex() noexcept
		{
			return _extensions_mutex;
		}

		const BaseRouter::extension_list& BaseRouter::getExtensions() const
		{
			return _extensions;
		}

		void BaseRouter::clearExtensions()
		{
			ibrcommon::RWLock l(_extensions_mutex);

			// delete all extensions
			for (extension_list::iterator iter = _extensions.begin(); iter != _extensions.end(); ++iter)
			{
				delete (*iter);
			}

			_extensions.clear();
		}

		void BaseRouter::extensionsUp() noexcept
		{
			ibrcommon::MutexLock l(_extensions_mutex);

			_nh_extension.componentUp();
			_retransmission_extension.componentUp();

			for (extension_list::iterator iter = _extensions.begin(); iter != _extensions.end(); ++iter)
			{
				RoutingExtension &ex = (**iter);
				ex.componentUp();
			}

			_extension_state = true;

			// trigger all routing modules to react to initial topology
			const std::set<dtn::core::Node> nl = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

			for (std::set<dtn::core::Node>::const_iterator iter = nl.begin(); iter != nl.end(); ++iter)
			{
				const dtn::core::Node &n = (*iter);

				// trigger all routing modules to search for bundles to forward
				__eventDataChanged( n.getEID() );
			}
		}

		void BaseRouter::extensionsDown() noexcept
		{
			ibrcommon::MutexLock l(_extensions_mutex);

			_extension_state = false;

			// stop all extensions
			for (extension_list::iterator iter = _extensions.begin(); iter != _extensions.end(); ++iter)
			{
				RoutingExtension &ex = (**iter);
				ex.componentDown();
			}

			_retransmission_extension.componentDown();
			_nh_extension.componentDown();
		}

		void BaseRouter::processHandshake(const dtn::data::EID &source, NodeHandshake &answer)
		{
			ibrcommon::MutexLock l(getExtensionMutex());

			// walk through all extensions to process the contents of the response
			const BaseRouter::extension_list& extensions = getExtensions();

			// process this handshake using the NodeHandshakeExtension
			_nh_extension.processHandshake(source, answer);

			// process this handshake using the retransmission extension
			_retransmission_extension.processHandshake(source, answer);

			for (BaseRouter::extension_list::const_iterator iter = extensions.begin(); iter != extensions.end(); ++iter)
			{
				RoutingExtension &extension = (**iter);
				extension.processHandshake(source, answer);
			}
		}

		void BaseRouter::responseHandshake(const dtn::data::EID &source, const NodeHandshake &request, NodeHandshake &answer)
		{
			ibrcommon::MutexLock l(getExtensionMutex());

			// walk through all extensions to process the contents of the response
			const BaseRouter::extension_list& extensions = getExtensions();

			// process this handshake using the NodeHandshakeExtension
			_nh_extension.responseHandshake(source, request, answer);

			// process this handshake using the retransmission extension
			_retransmission_extension.responseHandshake(source, request, answer);

			for (BaseRouter::extension_list::const_iterator iter = extensions.begin(); iter != extensions.end(); ++iter)
			{
				RoutingExtension &extension = (**iter);
				extension.responseHandshake(source, request, answer);
			}
		}

		void BaseRouter::requestHandshake(const dtn::data::EID &destination, NodeHandshake &request)
		{
			ibrcommon::MutexLock l(getExtensionMutex());

			// walk through all extensions to process the contents of the response
			const BaseRouter::extension_list& extensions = getExtensions();

			// process this handshake using the NodeHandshakeExtension
			_nh_extension.requestHandshake(destination, request);

			// process this handshake using the retransmission extension
			_retransmission_extension.requestHandshake(destination, request);

			for (BaseRouter::extension_list::const_iterator iter = extensions.begin(); iter != extensions.end(); ++iter)
			{
				RoutingExtension &extension = (**iter);
				extension.requestHandshake(destination, request);
			}
		}

		void BaseRouter::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013
			dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::add(this);
			dtn::core::EventDispatcher<dtn::net::TransferCompletedEvent>::add(this);
			dtn::core::EventDispatcher<dtn::net::BundleReceivedEvent>::add(this);
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::net::ConnectionEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::BundlePurgeEvent>::add(this);
		}

		void BaseRouter::componentDown() noexcept
		{
			// routine checked for noexcept on 15.02.2013
			dtn::core::EventDispatcher<dtn::net::TransferAbortedEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::net::TransferCompletedEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::net::BundleReceivedEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::NodeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::net::ConnectionEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::BundlePurgeEvent>::remove(this);
		}

		/**
		 * method to receive new events from the EventSwitch
		 */
		void BaseRouter::raiseEvent(const dtn::net::TransferCompletedEvent &event) noexcept
		{
			// if a transfer is completed, then release the transfer resource of the peer
			try {
				// lock the list of neighbors
				ibrcommon::MutexLock l(_neighbor_database);
				NeighborDatabase::NeighborEntry &entry = _neighbor_database.get(event.getPeer());
				entry.releaseTransfer(event.getBundle());

				// add the bundle to the summary vector of the neighbor
				entry.add(event.getBundle());
			} catch (const NeighborDatabase::EntryNotFoundException&) { };

			// trigger all routing modules to search for bundles to forward
			__eventTransferCompleted(event.getPeer(), event.getBundle());

			// trigger all routing modules to search for bundles to forward
			__eventDataChanged(event.getPeer());
		}

		void BaseRouter::raiseEvent(const dtn::routing::QueueBundleEvent &event) noexcept
		{
			// check Scope Control Block - do not forward bundles with hop limit == 0
			if (event.bundle.hopcount == 0) return;

			// if bundles are addressed to singletons ...
			if (event.bundle.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
			{
				// ... do not them if they are address to the local EID
				if (event.bundle.destination.sameHost(dtn::core::BundleCore::local)) return;
			}

			// If an incoming bundle is received, forward it to all connected neighbors
			// trigger all routing modules to forward the new bundle
			__eventBundleQueued(event.origin, event.bundle);
		}

		void BaseRouter::raiseEvent(const dtn::net::BundleReceivedEvent &event) noexcept
		{
			const dtn::data::MetaBundle m = dtn::data::MetaBundle::create(event.bundle);

			// Store incoming bundles into the storage
			try {
				if (event.fromlocal)
				{
					// store the bundle into a storage module
					getStorage().store(event.bundle);

					// set the bundle as known
					setKnown(m);

					// raise the queued event to notify all receivers about the new bundle
					QueueBundleEvent::raise(m, event.peer);
				}
				// if the bundle is not known
				else if (!filterKnown(m))
				{
					// security methods modifies the bundle, thus we need a copy of it
					dtn::data::Bundle bundle = event.bundle;

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

					// prevent loops
					try {
						ibrcommon::MutexLock l(_neighbor_database);

						// add the bundle to the summary vector of the neighbor
						_neighbor_database.get(event.peer).add(m);
					} catch (const NeighborDatabase::EntryNotFoundException&) { };

					// store the bundle into a storage module
					getStorage().store(bundle);

					// raise the queued event to notify all receivers about the new bundle
					QueueBundleEvent::raise(m, event.peer);
				}
				else
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(BaseRouter::TAG, 5) << "Duplicate bundle " << event.bundle.toString() << " from " << event.peer.getString() << " ignored." << IBRCOMMON_LOGGER_ENDL;
				}

				// finally create a bundle received event
				dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_RECEIVED);
#ifdef IBRDTN_SUPPORT_BSP
			} catch (const dtn::security::VerificationFailedException &ex) {
				IBRCOMMON_LOGGER_TAG(BaseRouter::TAG, notice) << "Security checks failed (" << ex.what() << "), bundle will be dropped: " << event.bundle.toString() << IBRCOMMON_LOGGER_ENDL;
#endif
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_TAG(BaseRouter::TAG, notice) << "Unable to store bundle " << event.bundle.toString() << IBRCOMMON_LOGGER_ENDL;

				// raise BundleEvent because we have to drop the bundle
				dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::DEPLETED_STORAGE);
			} catch (const dtn::storage::BundleStorage::StorageSizeExeededException &ex) {
				IBRCOMMON_LOGGER_TAG(BaseRouter::TAG, notice) << "No space left for bundle " << event.bundle.toString() << IBRCOMMON_LOGGER_ENDL;

				// raise BundleEvent because we have to drop the bundle
				dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::DEPLETED_STORAGE);
			} catch (const ibrcommon::Exception &ex) {
				IBRCOMMON_LOGGER_TAG(BaseRouter::TAG, error) << "Bundle " << event.bundle.toString() << " dropped: " << ex.what() << IBRCOMMON_LOGGER_ENDL;

				// raise BundleEvent because we have to drop the bundle
				dtn::core::BundleEvent::raise(m, dtn::core::BUNDLE_DELETED, dtn::data::StatusReportBlock::DEPLETED_STORAGE);
			}
		}

		void BaseRouter::raiseEvent(const dtn::net::TransferAbortedEvent &event) noexcept
		{
			// if a transfer is aborted, then release the transfer resource of the peer
			try {
				// lock the list of neighbors
				ibrcommon::MutexLock l(_neighbor_database);
				NeighborDatabase::NeighborEntry &entry = _neighbor_database.get(event.getPeer());
				entry.releaseTransfer(event.getBundleID());

				if (event.reason == dtn::net::TransferAbortedEvent::REASON_REFUSED)
				{
					const dtn::data::MetaBundle meta = getStorage().info(event.getBundleID());

					// add the transferred bundle to the bloomfilter of the receiver
					entry.add(meta);
				}
				else if (event.reason == dtn::net::TransferAbortedEvent::REASON_REFUSED_BY_FILTER)
				{
					const dtn::data::MetaBundle meta = getStorage().info(event.getBundleID());

					// add the bundle to the bloomfilter of the receiver to avoid further retries
					entry.add(meta);
				}
			} catch (const NeighborDatabase::EntryNotFoundException&) {
			} catch (const dtn::storage::NoBundleFoundException&) { };

			// trigger all routing modules to search for bundles to forward
			__eventDataChanged(event.getPeer());
		}

		void BaseRouter::raiseEvent(const dtn::core::NodeEvent &event) noexcept
		{
			// If a new neighbor comes available, send him a request for the summary vector
			// If a neighbor went away we can free the stored database
			if (event.getAction() == NODE_AVAILABLE)
			{
				{
					ibrcommon::MutexLock l(_neighbor_database);
					_neighbor_database.create( event.getNode().getEID() );
				}

				// trigger all routing modules to search for bundles to forward
				__eventDataChanged(event.getNode().getEID());
			}
			else if (event.getAction() == NODE_DATA_ADDED)
			{
				// trigger all routing modules to search for bundles to forward
				__eventDataChanged(event.getNode().getEID());
			}
			else if (event.getAction() == NODE_UNAVAILABLE)
			{
				try {
					ibrcommon::MutexLock l(_neighbor_database);
					_neighbor_database.get( event.getNode().getEID() ).reset();
				} catch (const NeighborDatabase::EntryNotFoundException&) { };

				// new bundles trigger a re-check for all neighbors
				const std::set<dtn::core::Node> nl = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

				for (std::set<dtn::core::Node>::const_iterator iter = nl.begin(); iter != nl.end(); ++iter)
				{
					const dtn::core::Node &n = (*iter);

					// trigger all routing modules to search for bundles to forward
					__eventDataChanged( n.getEID() );
				}
			}
		}

		void BaseRouter::raiseEvent(const dtn::net::ConnectionEvent &event) noexcept
		{
			if (event.getState() == dtn::net::ConnectionEvent::CONNECTION_UP)
			{
				// create a neighbor entry if that does not exists
				{
					ibrcommon::MutexLock l(_neighbor_database);
					_neighbor_database.create( event.getNode().getEID() );
				}

				// trigger all routing modules to search for bundles to forward
				__eventDataChanged( event.getNode().getEID() );
			}
		}

		void BaseRouter::raiseEvent(const dtn::core::BundlePurgeEvent &event) noexcept
		{
			if ((event.reason == dtn::core::BundlePurgeEvent::DELIVERED) ||
				(event.reason == dtn::core::BundlePurgeEvent::ACK_RECIEVED))
			{
				// add the purged bundle to the purge vector
				setPurged(event.bundle);
			}
		}

		void BaseRouter::raiseEvent(const dtn::core::TimeEvent &event) noexcept
		{
			dtn::data::Timestamp expire_time = event.getTimestamp();

			// do the expiration only every 60 seconds
			if (expire_time > _next_expiration) {
				// store the next expiration time
				_next_expiration = expire_time + 60;

				// expire all bundles and neighbors one minute late
				if (expire_time <= 60) expire_time = 0;
				else expire_time -= 60;

				{
					ibrcommon::MutexLock l(_known_bundles_lock);
					_known_bundles.expire(expire_time);

					// sync known bundles to disk
					_known_bundles.sync();
				}

				{
					ibrcommon::MutexLock l(_purged_bundles_lock);
					_purged_bundles.expire(expire_time);

					// sync purged bundles to disk
					_purged_bundles.sync();
				}

				{
					ibrcommon::MutexLock l(_neighbor_database);

					// get all active neighbors
					const std::set<dtn::core::Node> neighbors = dtn::core::BundleCore::getInstance().getConnectionManager().getNeighbors();

					// touch all active neighbors
					for (std::set<dtn::core::Node>::const_iterator it = neighbors.begin(); it != neighbors.end(); ++it) {
						try {
							_neighbor_database.get( (*it).getEID() );
						} catch (const NeighborDatabase::EntryNotFoundException&) { };
					}

					// check all neighbor entries for expiration
					_neighbor_database.expire(event.getTimestamp());
				}
			}
		}

		void BaseRouter::__eventDataChanged(const dtn::data::EID &peer) noexcept
		{
			// do not forward the event if the extensions are down
			if (!_extension_state) return;

			_nh_extension.eventDataChanged(peer);
			_retransmission_extension.eventDataChanged(peer);

			// notify all underlying extensions
			for (extension_list::const_iterator iter = _extensions.begin(); iter != _extensions.end(); ++iter)
			{
				(*iter)->eventDataChanged(peer);
			}
		}

		void BaseRouter::__eventBundleQueued(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) noexcept
		{
			// do not forward the event if the extensions are down
			if (!_extension_state) return;

			_nh_extension.eventBundleQueued(peer, meta);
			_retransmission_extension.eventBundleQueued(peer, meta);

			// notify all underlying extensions
			for (extension_list::const_iterator iter = _extensions.begin(); iter != _extensions.end(); ++iter)
			{
				(*iter)->eventBundleQueued(peer, meta);
			}
		}

		void BaseRouter::__eventTransferCompleted(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) noexcept
		{
			// do not forward the event if the extensions are down
			if (!_extension_state) return;

			_nh_extension.eventTransferCompleted(peer, meta);
			_retransmission_extension.eventTransferCompleted(peer, meta);

			// notify all underlying extensions
			for (extension_list::const_iterator iter = _extensions.begin(); iter != _extensions.end(); ++iter)
			{
				(*iter)->eventTransferCompleted(peer, meta);
			}
		}

		void BaseRouter::doHandshake(const dtn::data::EID &eid)
		{
			_nh_extension.doHandshake(eid);
		}

		void BaseRouter::pushHandshakeUpdated(const NodeHandshakeItem::IDENTIFIER id)
		{
			_nh_extension.pushHandshakeUpdated(id);
		}

		dtn::storage::BundleStorage& BaseRouter::getStorage()
		{
			return dtn::core::BundleCore::getInstance().getStorage();
		}

		dtn::storage::BundleSeeker& BaseRouter::getSeeker()
		{
			return dtn::core::BundleCore::getInstance().getSeeker();
		}

		void BaseRouter::setKnown(const dtn::data::MetaBundle &meta)
		{
			ibrcommon::MutexLock l(_known_bundles_lock);
			return _known_bundles.add(meta);
		}

		// check if the bundle is known
		bool BaseRouter::isKnown(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_known_bundles_lock);
			return _known_bundles.has(id);
		}

		// check if a bundle is known
		// if the bundle is unkown add it to the known list and return
		// false
		bool BaseRouter::filterKnown(const dtn::data::MetaBundle &meta)
		{
			ibrcommon::MutexLock l(_known_bundles_lock);
			bool ret = _known_bundles.has(meta);
			if (!ret) _known_bundles.add(meta);

			return ret;
		}

		const dtn::data::BundleSet BaseRouter::getKnownBundles()
		{
			ibrcommon::MutexLock l(_known_bundles_lock);
			return _known_bundles;
		}

		// set the bundle as known
		bool BaseRouter::isPurged(const dtn::data::BundleID &id)
		{
			ibrcommon::MutexLock l(_purged_bundles_lock);
			return _purged_bundles.has(id);
		}

		void BaseRouter::setPurged(const dtn::data::MetaBundle &meta)
		{
			ibrcommon::MutexLock l(_purged_bundles_lock);
			return _purged_bundles.add(meta);
		}

		const dtn::data::BundleSet BaseRouter::getPurgedBundles()
		{
			ibrcommon::MutexLock l(_purged_bundles_lock);
			return _purged_bundles;
		}

		const std::string BaseRouter::getName() const
		{
			return "BaseRouter";
		}

		NeighborDatabase& BaseRouter::getNeighborDB()
		{
			return _neighbor_database;
		}
	}
}
