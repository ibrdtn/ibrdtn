/*
 * EventConnection.h
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

#ifndef EVENTCONNECTION_H_
#define EVENTCONNECTION_H_

#include "api/ClientHandler.h"
#include "core/EventReceiver.h"

#include "core/NodeEvent.h"
#include "core/GlobalEvent.h"
#include "core/CustodyEvent.h"
#include "net/BundleReceivedEvent.h"
#include "net/TransferAbortedEvent.h"
#include "net/TransferCompletedEvent.h"
#include "net/ConnectionEvent.h"
#include "routing/QueueBundleEvent.h"

namespace dtn
{
	namespace api
	{
		class EventConnection : public ProtocolHandler,
			public dtn::core::EventReceiver<dtn::core::NodeEvent>,
			public dtn::core::EventReceiver<dtn::core::GlobalEvent>,
			public dtn::core::EventReceiver<dtn::core::CustodyEvent>,
			public dtn::core::EventReceiver<dtn::net::BundleReceivedEvent>,
			public dtn::core::EventReceiver<dtn::net::TransferAbortedEvent>,
			public dtn::core::EventReceiver<dtn::net::TransferCompletedEvent>,
			public dtn::core::EventReceiver<dtn::net::ConnectionEvent>,
			public dtn::core::EventReceiver<dtn::routing::QueueBundleEvent>
		{
		public:
			EventConnection(ClientHandler &client, ibrcommon::socketstream &stream);
			virtual ~EventConnection();

			void run();
			void finally();
			void setup();
			void __cancellation() noexcept;

			void raiseEvent(const dtn::core::NodeEvent &evt) noexcept;
			void raiseEvent(const dtn::core::GlobalEvent &evt) noexcept;
			void raiseEvent(const dtn::core::CustodyEvent &evt) noexcept;
			void raiseEvent(const dtn::net::BundleReceivedEvent &evt) noexcept;
			void raiseEvent(const dtn::net::TransferAbortedEvent &evt) noexcept;
			void raiseEvent(const dtn::net::TransferCompletedEvent &evt) noexcept;
			void raiseEvent(const dtn::net::ConnectionEvent &evt) noexcept;
			void raiseEvent(const dtn::routing::QueueBundleEvent &evt) noexcept;

		private:
			ibrcommon::Mutex _mutex;
			bool _running;
		};
	} /* namespace api */
} /* namespace dtn */
#endif /* EVENTCONNECTION_H_ */
