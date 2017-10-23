/*
 * ConnectionEvent.cpp
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

#include "net/ConnectionEvent.h"
#include "core/EventDispatcher.h"

namespace dtn
{
	namespace net
	{
		ConnectionEvent::ConnectionEvent(State s, const dtn::core::Node &n)
		 : _state(s), _node(n)
		{

		}

		ConnectionEvent::~ConnectionEvent()
		{

		}

		void ConnectionEvent::raise(State s, const dtn::core::Node &n)
		{
			// raise the new event
			dtn::core::EventDispatcher<ConnectionEvent>::queue( new ConnectionEvent(s, n) );
		}

		const std::string ConnectionEvent::getName() const
		{
			return "ConnectionEvent";
		}


		std::string ConnectionEvent::getMessage() const
		{
			switch (_state)
			{
			case CONNECTION_UP:
				return "connection up " + _node.getEID().getString();
			case CONNECTION_DOWN:
				return "connection down " + _node.getEID().getString();
			case CONNECTION_TIMEOUT:
				return "connection timeout " + _node.getEID().getString();
			case CONNECTION_SETUP:
				return "connection setup " + _node.getEID().getString();
			}
			return "unknown event";
		}

		ConnectionEvent::State ConnectionEvent::getState() const
		{
			return _state;
		}

		const dtn::core::Node& ConnectionEvent::getNode() const
		{
			return _node;
		}
	}
}
