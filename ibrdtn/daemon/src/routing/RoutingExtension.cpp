/*
 * RoutingExtension.cpp
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
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

#include "routing/RoutingExtension.h"
#include "routing/BaseRouter.h"
#include "core/BundleCore.h"
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace routing
	{
		const std::string RoutingExtension::TAG = "RoutingExtension";

		RoutingResult::RoutingResult()
		{
		}

		RoutingResult::~RoutingResult()
		{
		}

		void RoutingResult::put(const dtn::data::MetaBundle &bundle) throw ()
		{
			put(bundle, dtn::core::Node::CONN_UNDEFINED);
		}

		void RoutingResult::put(const dtn::data::MetaBundle &bundle, const dtn::core::Node::Protocol p) throw ()
		{
			push_back(std::make_pair(bundle, p));
		}

		/**
		 * base implementation of the Extension class
		 */
		RoutingExtension::RoutingExtension()
		{ }

		RoutingExtension::~RoutingExtension()
		{ }

		BaseRouter& RoutingExtension::operator*()
		{
			return dtn::core::BundleCore::getInstance().getRouter();
		}

		/**
		 * Transfer one bundle to another node.
		 * @param destination The EID of the other node.
		 * @param id The ID of the bundle to transfer. This bundle must be stored in the storage.
		 */
		void RoutingExtension::transferTo(const dtn::data::EID &destination, const dtn::data::MetaBundle &meta, const dtn::core::Node::Protocol p)
		{
			// acquire the transfer of this bundle, could throw already in transit or no resource left exception
			{
				// lock the list of neighbors
				ibrcommon::MutexLock l((**this).getNeighborDB());

				// get the neighbor entry for the next hop
				NeighborDatabase::NeighborEntry &entry = (**this).getNeighborDB().get(destination, true);

				// acquire the transfer, could throw already in transit or no resource left exception
				entry.acquireTransfer(meta);
			}
			try{
				//create the transfer object
				dtn::net::BundleTransfer transfer(destination, meta, p);

				// transfer the bundle to the next hop
				dtn::core::BundleCore::getInstance().getConnectionManager().queue(transfer);

				IBRCOMMON_LOGGER_DEBUG_TAG(RoutingExtension::TAG, 20) << "bundle " << meta.toString() << " queued by " << getTag() << " for " << destination.getString() << " via protocol " << dtn::core::Node::toString(p) << IBRCOMMON_LOGGER_ENDL;
			} catch (const dtn::core::P2PDialupException&) {
				// the bundle transfer queues the bundle for retransmission, thus abort the query here
				throw NeighborDatabase::EntryNotFoundException();
			} catch (const ibrcommon::Exception &e) {
				// ignore any other error
			}
		}

		void RoutingExtension::eventTransferSlotChanged(const dtn::data::EID &peer) throw ()
		{
			// To stay compatible with old modules, trigger this event if the modules
			// does not handle the new event separately.
			eventDataChanged(peer);
		}

	} /* namespace routing */
} /* namespace dtn */
