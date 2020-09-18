/*
 * Component.h
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

#ifndef COMPONENT_H_
#define COMPONENT_H_

#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Mutex.h>

namespace dtn
{
	namespace daemon
	{
		class Component
		{
		public:
			/**
			 * Destructor of the component.
			 * This should be called after all components are terminated.
			 * @return
			 */
			virtual ~Component() = 0;

			/**
			 * Set up the component.
			 * At this stage no other components should be used.
			 */
			virtual void initialize() noexcept = 0;

			/**
			 * Start up the component.
			 * At this stage all other components are ready.
			 */
			virtual void startup() noexcept = 0;

			/**
			 * Terminate the component and do some cleanup stuff.
			 * All other components still exists, but may not serve signals.
			 */
			virtual void terminate() noexcept = 0;

			/**
			 * Return an identifier for this component
			 * @return
			 */
			virtual const std::string getName() const = 0;
		};

		/**
		 * Independent components are working in an own thread.
		 */
		class IndependentComponent : public Component, protected ibrcommon::JoinableThread
		{
		public:
			IndependentComponent();
			virtual ~IndependentComponent();

			virtual void initialize() noexcept;
			virtual void startup() noexcept;
			virtual void terminate() noexcept;

		protected:
			void run() noexcept;

			/**
			 * This method is called after componentDown() and should
			 * should guarantee that blocking calls in componentRun()
			 * will unblock.
			 */
			virtual void __cancellation() noexcept = 0;

			/**
			 * Is called in preparation of the component.
			 * Before componentRun() is called.
			 */
			virtual void componentUp() noexcept = 0;

			/**
			 * This is the run method. The component should loop in there
			 * until componentDown() or __cancellation() is called.
			 */
			virtual void componentRun() noexcept = 0;

			/**
			 * This method is called if the component should stop. Clean-up
			 * code should be inserted here.
			 */
			virtual void componentDown() noexcept = 0;
		};

		class IntegratedComponent : public Component
		{
		public:
			IntegratedComponent();
			virtual ~IntegratedComponent();

			virtual void initialize() noexcept;
			virtual void startup() noexcept;
			virtual void terminate() noexcept;

		protected:
			virtual void componentUp() noexcept = 0;
			virtual void componentDown() noexcept = 0;
		};
	}
}

#endif /* COMPONENT_H_ */
