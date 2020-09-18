/*
 * ApiServer.h
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

#ifndef APISERVER_H_
#define APISERVER_H_

#include "Component.h"
#include "api/Registration.h"
#include "api/ClientHandler.h"
#include "core/EventReceiver.h"
#include "storage/BundleSeeker.h"
#include "routing/QueueBundleEvent.h"
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/net/socket.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/Timer.h>

#include <set>
#include <list>

namespace dtn
{
	namespace api
	{
		class ApiServer : public dtn::daemon::IndependentComponent, public dtn::core::EventReceiver<dtn::routing::QueueBundleEvent>, public ApiServerInterface, public ibrcommon::TimerCallback
		{
			static const std::string TAG;

		public:
			ApiServer(const ibrcommon::File &socket);
			ApiServer(const ibrcommon::vinterface &net, int port = 4550);
			virtual ~ApiServer();

			/**
			 * @see Component::getName()
			 */
			virtual const std::string getName() const;

			void freeRegistration(Registration &reg);

			void raiseEvent(const dtn::routing::QueueBundleEvent &evt) noexcept;

			/**
			 * retrieve a registration for a given handle from the ApiServers registration list
			 * the registration is automatically set into the attached state
			 * @param handle the handle of the registration to return
			 * @exception Registration::AlreadyAttachedException the registration is already attached to a different client
			 * @exception Registration::NotFoundException no registration was found for the given handle
			 * @return the registration with the given handle
			 */
			Registration& getRegistration(const std::string &handle);

			/**
			 * Removes expired registrations.
			 * @exception ibrcommon::Timer::StopTimerException thrown, if the GarbageCollector should be stopped
			 * @see ibrcommon::TimerCallback::timeout(Timer*)
			 * @return the seconds till the GarbageCollector should be triggered next
			 */
			virtual size_t timeout(ibrcommon::Timer*);

		protected:
			void __cancellation() noexcept;

			virtual void connectionUp(ClientHandler *conn);
			virtual void connectionDown(ClientHandler *conn);

			void componentUp() noexcept;
			void componentRun() noexcept;
			void componentDown() noexcept;

		private:

			/**
			 * starts the garbage collector if there are persistent registrations and it is not yet running
			 */
			void startGarbageCollector();
			/**
			 * calculates when the next registration expires
			 * @exception ibrcommon::Timer::StopTimerException if no more registrations expire
			 * @return time in seconds until next expiry
			 */
			size_t nextRegistrationExpiry();

			ibrcommon::vsocket _sockets;
			bool _shutdown;

			typedef std::list<Registration*> registration_list;
			registration_list _registrations;

			typedef std::list<ClientHandler*> client_list;
			client_list _connections;

			ibrcommon::Mutex _connection_lock;
			ibrcommon::Mutex _registration_lock;
			ibrcommon::Timer _garbage_collector;
		};
	}
}

#endif /* APISERVER_H_ */
