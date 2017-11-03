/*
 * TransferCompletedEvent.cpp
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

#include "net/TransferCompletedEvent.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

namespace dtn
{
	namespace net
	{
		TransferCompletedEvent::TransferCompletedEvent(const dtn::data::EID peer, const dtn::data::MetaBundle &bundle)
		 : _peer(peer), _bundle(bundle)
		{

		}

		TransferCompletedEvent::~TransferCompletedEvent()
		{

		}

		void TransferCompletedEvent::raise(const dtn::data::EID peer, const dtn::data::MetaBundle &bundle)
		{
			// raise the new event
			dtn::core::EventDispatcher<TransferCompletedEvent>::queue( new TransferCompletedEvent(peer, bundle) );
		}

		const std::string TransferCompletedEvent::getName() const
		{
			return "TransferCompletedEvent";
		}

		const dtn::data::EID& TransferCompletedEvent::getPeer() const
		{
			return _peer;
		}

		const dtn::data::MetaBundle& TransferCompletedEvent::getBundle() const
		{
			return _bundle;
		}

		std::string TransferCompletedEvent::getMessage() const
		{
			return "transfer of bundle " + _bundle.toString() + " to " + _peer.getString() + " completed";
		}
	}
}
