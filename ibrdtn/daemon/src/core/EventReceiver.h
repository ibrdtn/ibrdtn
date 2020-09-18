/*
 * EventReceiver.h
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

#ifndef EVENTRECEIVER_H_
#define EVENTRECEIVER_H_

#include "core/Event.h"

namespace dtn
{
	namespace core
	{
		template<class E>
		class EventReceiver
		{
		public:
			virtual ~EventReceiver() { };
			virtual void raiseEvent(const E &evt) noexcept = 0;
		};
	}
}

#endif /* EVENTRECEIVER_H_ */
