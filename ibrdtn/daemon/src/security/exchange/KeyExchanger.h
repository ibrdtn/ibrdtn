/*
 * KeyExchanger.h
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *             Thomas Schrader <schrader.thomas@gmail.com>
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

#ifndef KEYEXCHANGER_H_
#define KEYEXCHANGER_H_

#include "Configuration.h"
#include "Component.h"
#include "core/AbstractWorker.h"
#include "core/EventReceiver.h"
#include "core/TimeEvent.h"
#include "security/exchange/KeyExchangeEvent.h"
#include "security/exchange/KeyExchangeSession.h"
#include "security/exchange/KeyExchangeData.h"
#include "security/exchange/KeyExchangeProtocol.h"

#include <ibrcommon/data/File.h>
#include <ibrcommon/thread/Queue.h>

#include <openssl/bn.h>

#include <string>
#include <sstream>
#include <iostream>

namespace dtn
{
	namespace security
	{
		class KeyExchanger : public KeyExchangeManager, public dtn::daemon::IndependentComponent, public dtn::core::AbstractWorker,
			public dtn::core::EventReceiver<dtn::security::KeyExchangeEvent>,
			public dtn::core::EventReceiver<dtn::core::TimeEvent>
		{
			static const std::string TAG;

			public:
				KeyExchanger();
				virtual ~KeyExchanger();

				/**
				 * This method is called after componentDown() and should
				 * should guarantee that blocking calls in componentRun()
				 * will unblock.
				 */
				virtual void __cancellation() noexcept;

				/**
				 * Is called in preparation of the component.
				 * Before componentRun() is called.
				 */
				virtual void componentUp() noexcept;

				/**
				 * This is the run method. The component should loop in there
				 * until componentDown() or __cancellation() is called.
				 */
				virtual void componentRun() noexcept;

				/**
				 * This method is called if the component should stop. Clean-up
				 * code should be inserted here.
				 */
				virtual void componentDown() noexcept;

				/**
				 * Return an identifier for this component
				 * @return
				 */
				virtual const std::string getName() const;

				/**
				 * Receives incoming bundles.
				 */
				virtual void callbackBundleReceived(const dtn::data::Bundle &b);

				/**
				 * Receives incoming events.
				 */
				virtual void raiseEvent(const dtn::security::KeyExchangeEvent &evt) noexcept;
				virtual void raiseEvent(const dtn::core::TimeEvent &evt) noexcept;

				/**
				 * KeyExchangeManager methods
				 */
				virtual void submit(KeyExchangeSession &session, const KeyExchangeData &data);
				virtual void finish(KeyExchangeSession &session);
				virtual void error(KeyExchangeSession &session, bool reportError);

			private:
				class Task
				{
					public:
						virtual ~Task() = 0;

						/**
						 * Execute the code associated with this task.
						 */
						virtual void execute(KeyExchanger &exchanger) noexcept = 0;
				};

				class ExchangeTask : public Task
				{
					public:
						ExchangeTask(const dtn::data::EID &peer, const dtn::security::KeyExchangeData &data);
						virtual ~ExchangeTask() {};

						/**
						 * Execute the code associated with this task.
						 */
						virtual void execute(KeyExchanger &exchanger) noexcept;

					private:
						const dtn::data::EID _peer;
						dtn::security::KeyExchangeData _data;
				};

				class ExpireTask : public Task
				{
					public:
						ExpireTask(const dtn::data::Timestamp timestamp);
						virtual ~ExpireTask() {};

						/**
						 * Execute the code associated with this task.
						 */
						virtual void execute(KeyExchanger &exchanger) noexcept;

					private:
						const dtn::data::Timestamp _timestamp;
				};

				KeyExchangeSession& createSession(KeyExchangeProtocol &p, const dtn::data::EID &peer);
				KeyExchangeSession& createSession(KeyExchangeProtocol &p, const dtn::data::EID &peer, const dtn::security::KeyExchangeData &data);
				KeyExchangeSession& getSession(const dtn::data::EID &peer, const dtn::security::KeyExchangeData &data) noexcept (false);
				void freeSession(const dtn::data::EID &peer, const unsigned int uniqueId);

				void expire(const dtn::data::Timestamp timestamp);

				// queue to hand-over object to the main-thread
				ibrcommon::Queue<KeyExchanger::Task*> _queue;

				std::map<std::string, KeyExchangeSession*> _sessionmap;

				std::map<int, KeyExchangeProtocol*> _protocols;

				ibrcommon::Mutex _expiration_lock;
				dtn::data::Timestamp _next_expiration;
		};
	} /* namespace security */
} /* namespace dtn */
#endif /* KEYEXCHANGER_H_ */
