/*
 * TestEventListener.h
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

#ifndef TESTEVENTLISTENER_H_
#define TESTEVENTLISTENER_H_

#include "core/EventReceiver.h"
#include "core/EventDispatcher.h"
#include <ibrcommon/thread/Conditional.h>

template <class EVENT>
class TestEventListener : public dtn::core::EventReceiver<EVENT> {
public:
	TestEventListener() : event_counter(0) {
		dtn::core::EventDispatcher<EVENT>::add(this);
	}

	virtual ~TestEventListener() {
		dtn::core::EventDispatcher<EVENT>::remove(this);
	}

	void raiseEvent(const EVENT&) noexcept {
		ibrcommon::MutexLock l(event_cond);
		event_counter++;
		event_cond.signal(true);
	}

	ibrcommon::Conditional event_cond;
	unsigned int event_counter;
};

#endif /* TESTEVENTLISTENER_H_ */
