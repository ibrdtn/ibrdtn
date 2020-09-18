/*
 * RetransmissionExtension.h
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

#ifndef RETRANSMISSIONEXTENSION_H_
#define RETRANSMISSIONEXTENSION_H_

#include "routing/RoutingExtension.h"

#include "core/BundleExpiredEvent.h"
#include "net/TransferAbortedEvent.h"
#include "routing/RequeueBundleEvent.h"
#include "core/TimeEvent.h"

#include "core/EventReceiver.h"
#include <ibrdtn/data/BundleID.h>
#include <ibrdtn/data/EID.h>
#include <ibrcommon/thread/Mutex.h>
#include <queue>
#include <set>

namespace dtn
{
	namespace routing
	{
		class RetransmissionExtension : public RoutingExtension,
			public dtn::core::EventReceiver<dtn::net::TransferAbortedEvent>,
			public dtn::core::EventReceiver<dtn::routing::RequeueBundleEvent>,
			public dtn::core::EventReceiver<dtn::core::BundleExpiredEvent>,
			public dtn::core::EventReceiver<dtn::core::TimeEvent>
		{
		public:
			RetransmissionExtension();
			virtual ~RetransmissionExtension();

			/**
			 * This method is called every time a bundle has been completed successfully
			 */
			virtual void eventTransferCompleted(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) noexcept;

			void raiseEvent(const dtn::net::TransferAbortedEvent &evt) noexcept;
			void raiseEvent(const dtn::routing::RequeueBundleEvent &evt) noexcept;
			void raiseEvent(const dtn::core::BundleExpiredEvent &evt) noexcept;
			void raiseEvent(const dtn::core::TimeEvent &evt) noexcept;

			void componentUp() noexcept;
			void componentDown() noexcept;

		private:
			class RetransmissionData : public dtn::data::BundleID
			{
			public:
				RetransmissionData(const dtn::data::BundleID &id, const dtn::data::EID &destination, dtn::core::Node::Protocol p = dtn::core::Node::CONN_UNDEFINED, const dtn::data::Size retry = 2);
				virtual ~RetransmissionData();


				const dtn::data::EID destination;
				const dtn::core::Node::Protocol protocol;

				const dtn::data::Timestamp& getTimestamp() const;
				dtn::data::Size getCount() const;

				RetransmissionData& operator++();
				RetransmissionData& operator++(int);

				bool operator!=(const RetransmissionData &obj);
				bool operator==(const RetransmissionData &obj);

			private:
				dtn::data::Timestamp _timestamp;
				dtn::data::Size _count;
				const dtn::data::Size retry;
			};

			ibrcommon::Mutex _mutex;
			std::queue<RetransmissionData> _queue;
			std::set<RetransmissionData> _set;
		};
	}
}

#endif /* RETRANSMISSIONEXTENSION_H_ */
