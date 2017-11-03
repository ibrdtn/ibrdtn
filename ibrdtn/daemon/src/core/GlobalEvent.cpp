/*
 * GlobalEvent.h
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

#include "core/GlobalEvent.h"
#include "core/EventDispatcher.h"


namespace dtn
{
	namespace core
	{
		GlobalEvent::GlobalEvent(const Action a)
		 : _action(a)
		{
		}

		GlobalEvent::~GlobalEvent()
		{
		}

		GlobalEvent::Action GlobalEvent::getAction() const
		{
			return _action;
		}

		const std::string GlobalEvent::getName() const
		{
			return "GlobalEvent";
		}

		std::string GlobalEvent::getMessage() const
		{
			switch (getAction()) {
			case GlobalEvent::GLOBAL_SHUTDOWN:
				return "Shutdown initiated.";
			case GlobalEvent::GLOBAL_RELOAD:
				return "Reload signal received.";
			case GlobalEvent::GLOBAL_IDLE:
				return "Switched to IDLE mode.";
			case GlobalEvent::GLOBAL_BUSY:
				return "Switched to BUSY mode.";
			case GlobalEvent::GLOBAL_LOW_ENERGY:
				return "Switched to low-energy mode.";
			case GlobalEvent::GLOBAL_NORMAL:
				return "Switched back to normal operations.";
			case GlobalEvent::GLOBAL_START_DISCOVERY:
				return "Start peer discovery.";
			case GlobalEvent::GLOBAL_STOP_DISCOVERY:
				return "Stop peer discovery.";
			case GlobalEvent::GLOBAL_INTERNET_AVAILABLE:
				return "Internet connection is available.";
			case GlobalEvent::GLOBAL_INTERNET_UNAVAILABLE:
				return "Internet connection is gone.";
			default:
				return "unknown";
			}

			return "unknown";
		}

		void GlobalEvent::raise(const Action a)
		{
			// raise the new event
			dtn::core::EventDispatcher<GlobalEvent>::queue( new GlobalEvent(a) );
		}
	}
}
