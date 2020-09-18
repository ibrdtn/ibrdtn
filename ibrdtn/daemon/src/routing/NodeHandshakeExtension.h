/*
 * NodeHandshakeExtension.h
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

#ifndef NODEHANDSHAKEEXTENSION_H_
#define NODEHANDSHAKEEXTENSION_H_

#include "routing/RoutingExtension.h"
#include "core/AbstractWorker.h"
#include "core/EventReceiver.h"
#include "core/NodeEvent.h"

#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/Queue.h>
#include <map>

namespace dtn
{
	namespace routing
	{
		class NodeHandshakeExtension : public RoutingExtension, public dtn::core::EventReceiver<dtn::core::NodeEvent>
		{
			static const std::string TAG;

		public:
			NodeHandshakeExtension();
			virtual ~NodeHandshakeExtension();

			void raiseEvent(const dtn::core::NodeEvent &evt) noexcept;
			void componentUp() noexcept;
			void componentDown() noexcept;

			void doHandshake(const dtn::data::EID &eid);

			void pushHandshakeUpdated(const NodeHandshakeItem::IDENTIFIER id);

			/**
			 * @see BaseRouter::requestHandshake()
			 */
			void requestHandshake(const dtn::data::EID &destination, NodeHandshake &request) const;

			/**
			 * @see BaseRouter::responseHandshake()
			 */
			void responseHandshake(const dtn::data::EID &source, const NodeHandshake &request, NodeHandshake &answer);

			/**
			 * @see BaseRouter::processHandshake()
			 */
			void processHandshake(const dtn::data::EID &source, NodeHandshake &answer);

		protected:
			void processHandshake(const dtn::data::Bundle &bundle);

		private:
			class HandshakeEndpoint : public dtn::core::AbstractWorker
			{
			public:
				HandshakeEndpoint(NodeHandshakeExtension &callback);
				virtual ~HandshakeEndpoint();

				void callbackBundleReceived(const Bundle &b);
				void query(const dtn::data::EID &eid);
				void push(const NodeHandshakeItem::IDENTIFIER id);

				void send(dtn::data::Bundle &b);

				void removeFromBlacklist(const dtn::data::EID &eid);

			private:
				NodeHandshakeExtension &_callback;
				std::map<dtn::data::EID, dtn::data::Timestamp> _blacklist;
				ibrcommon::Mutex _blacklist_lock;
			};

			/**
			 * process the incoming bundles and send messages to other routers
			 */
			HandshakeEndpoint _endpoint;

			static const dtn::data::EID BROADCAST_ENDPOINT;
		};
	} /* namespace routing */
} /* namespace dtn */
#endif /* NODEHANDSHAKEEXTENSION_H_ */
