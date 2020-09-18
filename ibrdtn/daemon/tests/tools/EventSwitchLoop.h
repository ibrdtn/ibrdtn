/*
 * EventSwitchLoop.h
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

#include "core/EventSwitch.h"
#include "core/GlobalEvent.h"
#include <ibrcommon/thread/Thread.h>

#ifndef EVENTSWITCHLOOP_H_
#define EVENTSWITCHLOOP_H_

namespace ibrtest
{
	class EventSwitchLoop : public ibrcommon::JoinableThread
	{
	public:
		EventSwitchLoop()
		{
			dtn::core::EventSwitch &es = dtn::core::EventSwitch::getInstance();
			es.initialize();
		};

		virtual ~EventSwitchLoop()
		{
			dtn::core::EventSwitch &es = dtn::core::EventSwitch::getInstance();
			es.terminate();

			join();
		};

	protected:
		virtual void run() noexcept
		{
			dtn::core::EventSwitch &es = dtn::core::EventSwitch::getInstance();
			es.loop(0);
		}

		virtual void __cancellation() noexcept
		{
			dtn::core::EventSwitch &es = dtn::core::EventSwitch::getInstance();
			es.shutdown();
		}
	};
}

#endif /* EVENTSWITCHLOOP_H_ */
