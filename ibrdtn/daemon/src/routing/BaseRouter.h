/*
 * BaseRouter.h
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

#ifndef BASEROUTER_H_
#define BASEROUTER_H_

#include "Component.h"
#include "routing/NeighborDatabase.h"
#include "routing/NodeHandshake.h"
#include "core/EventReceiver.h"
#include "storage/BundleStorage.h"
#include "storage/BundleSeeker.h"

#include "routing/RoutingExtension.h"
#include "routing/NodeHandshakeExtension.h"
#include "routing/RetransmissionExtension.h"

#include <ibrdtn/data/BundleSet.h>
#include <ibrdtn/data/BundleID.h>
#include <ibrdtn/data/MetaBundle.h>

#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/RWMutex.h>
#include <ibrcommon/thread/Conditional.h>

#include "net/TransferAbortedEvent.h"
#include "net/TransferCompletedEvent.h"
#include "routing/QueueBundleEvent.h"
#include "core/NodeEvent.h"
#include "core/TimeEvent.h"
#include "net/ConnectionEvent.h"
#include "core/BundlePurgeEvent.h"




namespace dtn
{
	namespace routing
	{
		class BaseRouter : public dtn::daemon::IntegratedComponent,
			public dtn::core::EventReceiver<dtn::net::TransferAbortedEvent>,
			public dtn::core::EventReceiver<dtn::net::TransferCompletedEvent>,
			public dtn::core::EventReceiver<dtn::routing::QueueBundleEvent>,
			public dtn::core::EventReceiver<dtn::core::NodeEvent>,
			public dtn::core::EventReceiver<dtn::core::TimeEvent>,
			public dtn::core::EventReceiver<dtn::net::ConnectionEvent>,
			public dtn::core::EventReceiver<dtn::core::BundlePurgeEvent>
		{
			static const std::string TAG;

		public:
			class RoutingException : public ibrcommon::Exception
			{
				public:
					RoutingException(std::string what = "An error occured during routing.") throw() : Exception(what)
					{
					};
			};

			/**
			 * If no neighbour was found, this exception is thrown.
			 */
			class NoNeighbourFoundException : public RoutingException
			{
				public:
					NoNeighbourFoundException(std::string what = "No neighbour was found.") throw() : RoutingException(what)
					{
					};
			};

			/**
			 * If no route can be found, this exception is thrown.
			 */
			class NoRouteFoundException : public RoutingException
			{
				public:
					NoRouteFoundException(std::string what = "No route found.") throw() : RoutingException(what)
					{
					};
			};

			typedef std::set<RoutingExtension*> extension_list;

			BaseRouter();
			~BaseRouter();

			/**
			 * Add a routing extension to the routing core.
			 * @param extension
			 */
			void add(RoutingExtension *extension);

			/**
			 * Remove a routing extension from the routing core
			 */
			void remove(RoutingExtension *extension);

			/**
			 * Returns a reference to all extensions.
			 * @return
			 */
			const extension_list& getExtensions() const;

			/**
			 * Give access to the mutex for the extension list
			 */
			ibrcommon::RWMutex& getExtensionMutex() throw ();

			/**
			 * Delete all extensions
			 */
			void clearExtensions();

			/**
			 * method to receive new events from the EventSwitch
			 */
			void raiseEvent(const dtn::net::TransferAbortedEvent &evt) throw ();
			void raiseEvent(const dtn::net::TransferCompletedEvent &evt) throw ();
			void raiseEvent(const dtn::routing::QueueBundleEvent &evt) throw ();
			void raiseEvent(const dtn::core::NodeEvent &evt) throw ();
			void raiseEvent(const dtn::core::TimeEvent &evt) throw ();
			void raiseEvent(const dtn::net::ConnectionEvent &evt) throw ();
			void raiseEvent(const dtn::core::BundlePurgeEvent &evt) throw ();

			/**
			 * provides direct access to the bundle storage
			 */
			dtn::storage::BundleStorage &getStorage();

			/**
			 * provides direct access to the bundle seeker
			 */
			dtn::storage::BundleSeeker &getSeeker();

			/**
			 * Request a neighbor handshake
			 * @param eid
			 */
			void doHandshake(const dtn::data::EID &eid);

			/**
			 * Signals that some important data of the handshake has been updated
			 */
			void pushHandshakeUpdated(const NodeHandshakeItem::IDENTIFIER id);

			/**
			 * This method returns true, if the given BundleID is known.
			 * @param id
			 * @return
			 */
			bool isKnown(const dtn::data::BundleID &id);

			/**
			 * check if a bundle is known
			 * if the bundle is unkown add it to the known list and return false
			 * @param id
			 * @return
			 */
			bool filterKnown(const dtn::data::MetaBundle &meta);

			/**
			 * This method add a BundleID to the set of known bundles
			 * @param id
			 */
			void setKnown(const dtn::data::MetaBundle &meta);

			/**
			 * Get a vector (bloomfilter) of all known bundles.
			 * @return
			 */
			const dtn::data::BundleSet getKnownBundles();

			/**
			 * Get a vector (bloomfilter) of all purged bundles.
			 * @return
			 */
			const dtn::data::BundleSet getPurgedBundles();

			/**
			 * Add a bundle to the purge vector of this daemon.
			 * @param meta The bundle to purge.
			 */
			void setPurged(const dtn::data::MetaBundle &meta);

			/**
			 * This method returns true, if the given BundleID is purged.
			 * @param id
			 * @return
			 */
			bool isPurged(const dtn::data::BundleID &id);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			/**
			 * Access to the neighbor database. Where several data about the neighbors is stored.
			 * @return
			 */
			NeighborDatabase& getNeighborDB();

			/**
			 * enable all extensions
			 */
			void extensionsUp() throw ();

			/**
			 * disable all extensions
			 */
			void extensionsDown() throw ();

			/**
			 * Process a completed handshake
			 * This method will iterate through all extensions and give each of them
			 * a chance to process the completed handshake
			 */
			void processHandshake(const dtn::data::EID &source, NodeHandshake &answer);

			/**
			 * Respond to a received handshake request
			 * This method will iterate through all extensions and give each of them
			 * a chance to respond to a received handshake request
			 */
			void responseHandshake(const dtn::data::EID&, const NodeHandshake&, NodeHandshake&);

			/**
			 * Request to a received handshake request
			 * This method will iterate through all extensions and ask each of them
			 * if they are interested in some sort of routing data of the other peer
			 */
			void requestHandshake(const dtn::data::EID &destination, NodeHandshake &request);


		protected:
			virtual void componentUp() throw ();
			virtual void componentDown() throw ();

		private:
			void __eventDataChanged(const dtn::data::EID &peer) throw ();
			void __eventTransferSlotChanged(const dtn::data::EID &peer) throw ();
			void __eventTransferCompleted(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) throw ();
			void __eventBundleQueued(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) throw ();

			ibrcommon::Mutex _known_bundles_lock;
			dtn::data::BundleSet _known_bundles;

			ibrcommon::Mutex _purged_bundles_lock;
			dtn::data::BundleSet _purged_bundles;

			ibrcommon::RWMutex _extensions_mutex;
			extension_list _extensions;

			// this is true if the extensions are up
			bool _extension_state;

			NeighborDatabase _neighbor_database;
			NodeHandshakeExtension _nh_extension;
			RetransmissionExtension _retransmission_extension;

			dtn::data::Timestamp _next_expiration;
		};
	}
}


#endif /* BASEROUTER_H_ */
