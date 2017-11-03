/*
 * RequeueBundleEvent.cpp
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

#include "routing/RequeueBundleEvent.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

namespace dtn
{
	namespace routing
	{
		RequeueBundleEvent::RequeueBundleEvent(const dtn::data::EID peer, const dtn::data::BundleID &id, dtn::core::Node::Protocol p)
		 : _peer(peer), _bundle(id), _protocol(p)
		{

		}

		RequeueBundleEvent::~RequeueBundleEvent()
		{

		}

		void RequeueBundleEvent::raise(const dtn::data::EID peer, const dtn::data::BundleID &id, dtn::core::Node::Protocol p)
		{
			// raise the new event
			dtn::core::EventDispatcher<RequeueBundleEvent>::queue( new RequeueBundleEvent(peer, id, p) );
		}

		const std::string RequeueBundleEvent::getName() const
		{
			return "RequeueBundleEvent";
		}

		std::string RequeueBundleEvent::getMessage() const
		{
			return "Bundle requeued " + _bundle.toString();
		}

		const dtn::data::EID& RequeueBundleEvent::getPeer() const
		{
			return _peer;
		}

		const dtn::data::BundleID& RequeueBundleEvent::getBundle() const
		{
			return _bundle;
		}

		dtn::core::Node::Protocol RequeueBundleEvent::getProtocol() const
		{
			return _protocol;
		}
	}
}
