/*
 * TransferAbortedEvent.cpp
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

#include "net/TransferAbortedEvent.h"
#include "core/EventDispatcher.h"

namespace dtn
{
	namespace net
	{
		TransferAbortedEvent::TransferAbortedEvent(const dtn::data::EID &peer, const dtn::data::BundleID &id, const AbortReason r)
		 : reason(r), _peer(peer), _bundle(id)
		{
		}

		TransferAbortedEvent::~TransferAbortedEvent()
		{

		}

		void TransferAbortedEvent::raise(const dtn::data::EID &peer, const dtn::data::BundleID &id, const AbortReason r)
		{
			// raise the new event
			dtn::core::EventDispatcher<TransferAbortedEvent>::queue( new TransferAbortedEvent(peer, id, r) );
		}

		const std::string TransferAbortedEvent::getName() const
		{
			return "TransferAbortedEvent";
		}

		const dtn::data::EID& TransferAbortedEvent::getPeer() const
		{
			return _peer;
		}

		const dtn::data::BundleID& TransferAbortedEvent::getBundleID() const
		{
			return _bundle;
		}

		const std::string TransferAbortedEvent::getReason(const AbortReason reason)
		{
			switch (reason)
			{
			case REASON_UNDEFINED:
				return "undefined";

			case REASON_CONNECTION_DOWN:
				return "connection went down";

			case REASON_REFUSED:
				return "bundle has been refused";

			case REASON_RETRY_LIMIT_REACHED:
				return "retry limit reached";

			case REASON_BUNDLE_DELETED:
				return "bundle has been deleted";

			case REASON_REFUSED_BY_FILTER:
				return "bundle has been rejected by filtering directives";
			}
			return "undefined";
		}

		std::string TransferAbortedEvent::getMessage() const
		{
			return "transfer of bundle " + _bundle.toString() + " to " + _peer.getString() + " aborted. (" + getReason(reason) + ")";
		}
	}
}
