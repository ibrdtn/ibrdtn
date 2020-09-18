/*
 * AbstractWorker.h
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

#ifndef ABSTRACTWORKER_H_
#define ABSTRACTWORKER_H_

#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/BundleID.h>
#include <ibrdtn/data/EID.h>
#include "core/EventReceiver.h"
#include "routing/QueueBundleEvent.h"
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/Conditional.h>
#include <ibrcommon/thread/Thread.h>
#include "net/ConvergenceLayer.h"

#include <ibrcommon/thread/Queue.h>
#include <set>

using namespace dtn::data;

namespace dtn
{
	namespace core
	{
		class AbstractWorker : public ibrcommon::Mutex
		{
			class AbstractWorkerAsync : public ibrcommon::JoinableThread, public dtn::core::EventReceiver<dtn::routing::QueueBundleEvent>
			{
			public:
				AbstractWorkerAsync(AbstractWorker &worker);
				virtual ~AbstractWorkerAsync();

				void initialize();
				void shutdown();

				virtual void raiseEvent(const dtn::routing::QueueBundleEvent &evt) noexcept;

			protected:
				void run() noexcept;
				void __cancellation() noexcept;

			private:
				AbstractWorker &_worker;
				bool _running;

				ibrcommon::Queue<dtn::data::BundleID> _receive_bundles;
			};

			public:
				AbstractWorker();

				virtual ~AbstractWorker();

				virtual const EID getWorkerURI() const;

				virtual void callbackBundleReceived(const Bundle &b) = 0;

			protected:
				void initialize(const std::string &uri);
				void transmit(dtn::data::Bundle &bundle);

				dtn::data::EID _eid;

				void shutdown();

				/**
				 * subscribe to a end-point
				 */
				void subscribe(const dtn::data::EID &endpoint);

				/**
				 * unsubscribe to a end-point
				 */
				void unsubscribe(const dtn::data::EID &endpoint);

			private:
				AbstractWorkerAsync _thread;
				std::set<dtn::data::EID> _groups;
		};
	}
}

#endif /*ABSTRACTWORKER_H_*/
