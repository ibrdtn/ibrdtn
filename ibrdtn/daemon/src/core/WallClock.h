/*
 * Clock.h
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

#ifndef WALLCLOCK_H_
#define WALLCLOCK_H_

#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/thread/Timer.h>
#include <ibrdtn/data/Number.h>
#include "Component.h"

namespace dtn
{
	namespace core
	{
		class WallClock : public ibrcommon::Conditional, public dtn::daemon::IntegratedComponent, public ibrcommon::TimerCallback
		{
		public:
			/**
			 * Constructor for the global Clock
			 * @param frequency Specify the frequency for the clock tick in seconds.
			 */
			WallClock(const dtn::data::Timeout &frequency);
			virtual ~WallClock();

			/**
			 * Blocks until the next clock tick happens.
			 */
			void sync();

			/**
			 * timer callback method
			 * @see TimerCallback::timeout()
			 */
			virtual size_t timeout(ibrcommon::Timer*);

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

		protected:
			virtual void componentUp() noexcept;
			virtual void componentDown() noexcept;

		private:
			dtn::data::Timeout _frequency;
			size_t _next;
			ibrcommon::Timer _timer;

		};
	}
}

#endif /* WALLCLOCK_H_ */
