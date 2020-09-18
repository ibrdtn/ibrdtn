/*
 * Component.cpp
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

#include "Component.h"
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace daemon
	{
		Component::~Component()
		{
		}

		IndependentComponent::IndependentComponent()
		{
		}

		IndependentComponent::~IndependentComponent()
		{
			join();
		}

		void IndependentComponent::initialize() noexcept
		{
			// reset thread if necessary
			if (JoinableThread::isFinalized()) JoinableThread::reset();

			componentUp();
		}

		void IndependentComponent::startup() noexcept
		{
			try {
				JoinableThread::start();
			} catch (const ibrcommon::ThreadException &ex) {
				IBRCOMMON_LOGGER_TAG("IndependentComponent", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void IndependentComponent::terminate() noexcept
		{
			componentDown();
			JoinableThread::stop();
		}

		void IndependentComponent::run() noexcept
		{
			componentRun();
		}

		IntegratedComponent::IntegratedComponent()
		{
		}

		IntegratedComponent::~IntegratedComponent()
		{
		}

		void IntegratedComponent::initialize() noexcept
		{
			componentUp();
		}

		void IntegratedComponent::startup() noexcept
		{
			// nothing to do
		}

		void IntegratedComponent::terminate() noexcept
		{
			componentDown();
		}
	}
}
