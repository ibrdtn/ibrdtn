/*
 * Registration.h
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

#ifndef REGISTRATION_H_
#define REGISTRATION_H_

#include "storage/BundleStorage.h"
#include "storage/BundleResult.h"
#include <ibrdtn/data/BundleID.h>
#include <ibrdtn/data/BundleSet.h>
#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/thread/Timer.h>
#include <string>
#include <set>

namespace dtn
{
	namespace api
	{
		class Registration
		{
			static const std::string TAG;

		public:
			enum NOTIFY_CALL
			{
				NOTIFY_BUNDLE_AVAILABLE = 0,
				NOTIFY_NEIGHBOR_AVAILABLE = 1,
				NOTIFY_NEIGHBOR_UNAVAILABLE = 2,
				NOTIFY_SHUTDOWN = 3
			};

			class RegistrationException : public ibrcommon::Exception
			{
			public:
				RegistrationException(std::string what = "") throw() : Exception(what)
				{
				}
			};

			class AlreadyAttachedException : public RegistrationException
			{
			public:
				AlreadyAttachedException(std::string what = "") throw() : RegistrationException(what)
				{
				}
			};

			class NotFoundException : public RegistrationException
			{
			public:
				NotFoundException(std::string what = "") throw() : RegistrationException(what)
				{
				}
			};

			class NotPersistentException : public RegistrationException
			{
			public:
				NotPersistentException(std::string what = "") throw() : RegistrationException(what)
				{
				}
			};

			/**
			 * constructor of the registration
			 */
			Registration();

			/**
			 * create a registration with a pre-defined handle
			 * (used to resume a session with an external manager)
			 */
			Registration(const std::string &handle);

			/**
			 * destructor of the registration
			 */
			virtual ~Registration();

			/**
			 * notify the corresponding client about something happen
			 */
			void notify(const NOTIFY_CALL);

			/**
			 * wait for the next notify event
			 * @return
			 */
			NOTIFY_CALL wait();

			/**
			 * wait for available bundle
			 */
			void wait_for_bundle(size_t timeout = 0);

			/**
			 * subscribe to a end-point
			 */
			void subscribe(const dtn::data::EID &endpoint);

			/**
			 * unsubscribe to a end-point
			 */
			void unsubscribe(const dtn::data::EID &endpoint);

			/**
			 * check if this registration has subscribed to a specific endpoint
			 * @param endpoint
			 * @return
			 */
			bool hasSubscribed(const dtn::data::EID &endpoint);

			/**
			 * @return A list of active subscriptions.
			 */
			const std::set<dtn::data::EID> getSubscriptions();

			/**
			 * compares the local handle with the given one
			 */
			bool operator==(const std::string&) const;

			/**
			 * compares another registration with this one
			 */
			bool operator==(const Registration&) const;

			/**
			 * compares and order a registration (using the handle)
			 */
			bool operator<(const Registration&) const;

			/**
			 * Receive a bundle from the queue. If the queue is empty, it will
			 * query the storage for more bundles (up to 10). If no bundle is found
			 * and the queue is empty an exception is thrown.
			 * @return
			 */
			dtn::data::Bundle receive() throw (dtn::storage::NoBundleFoundException);

			dtn::data::MetaBundle receiveMetaBundle() throw (dtn::storage::NoBundleFoundException);

			/**
			 * notify a bundle as delivered (and delete it if singleton destination)
			 * @param id
			 */
			void delivered(const dtn::data::MetaBundle &m) const;

			/**
			 * returns a default EID based on the registration handle
			 * @return A EID object.
			 */
			const dtn::data::EID& getDefaultEID() const;

			/**
			 * returns the handle of this registration
			 * @return
			 */
			const std::string& getHandle() const;

			/**
			 * abort all blocking operations on this registration
			 */
			void abort();

			/**
			 * resets the underlying queues (if you want to continue
			 * using the registration after an abort call
			 */
			void reset();

			/**
			 * make this registration persistent
			 * @param lifetime the lifetime of the registration in seconds
			 */
			void setPersistent(ibrcommon::Timer::time_t lifetime);

			/**
			 * remove the persistent flag of this registration
			 */
			void unsetPersistent();

			/**
			 * returns the persistent state of this registration
			 * and also sets the persistent state to false, if the timer is expired
			 * @return the persistent state
			 */
			bool isPersistent();

			/**
			 * returns the persistent state of this registration
			 * @return the persistent state
			 */
			bool isPersistent() const;

			/**
			 * Allows to disable the re-assemble of fragments
			 */
			void setFilterFragments(bool val);

			/**
			 * gets the expire time of this registration if it is persistent
			 * @see ibrcommon::Timer::get_current_time()
			 * @exception NotPersistentException the registration is not persistent
			 * @return the expire time according to ibrcommon::Timer::get_current_time()
			 */
			ibrcommon::Timer::time_t getExpireTime() const;

			/**
			 * attaches a client to this registration
			 * @exception AlreadyAttachedException a client is already attached
			 */
			void attach();

			/**
			 * detaches a client from this registration
			 */
			void detach();

			/**
			 *
			 */
			static void processIncomingBundle(const dtn::data::EID &source, dtn::data::Bundle &bundle);

		protected:
			void underflow();

		private:
			class RegistrationQueue : public dtn::storage::BundleResult {
			public:
				/**
				 * Constructor
				 */
				RegistrationQueue();

				/**
				 * Destructor
				 */
				virtual ~RegistrationQueue();

				/**
				 * Put a bundle into the registration queue
				 * This method is used by the storage.
				 * @see dtn::storage::BundleResult::put()
				 */
				virtual void put(const dtn::data::MetaBundle &bundle) throw ();

				/**
				 * Get the next bundle of the queue.
				 * An exception is thrown if the queue is empty or the queue has been aborted
				 * before.
				 */
				dtn::data::MetaBundle pop() throw (const ibrcommon::QueueUnblockedException);

				/**
				 * Expire bundles in the received bundle set
				 */
				void expire(const dtn::data::Timestamp &timestamp) throw ();

				/**
				 * Abort all blocking call on the queue
				 */
				void abort() throw ();

				/**
				 * Reset the queue state. If called, blocking calls are allowed again.
				 */
				void reset() throw ();

				/**
				 * Check if a bundle has been received before
				 */
				bool has(const dtn::data::BundleID &bundle) const throw ();

			private:
				// protect variables against concurrent altering
				ibrcommon::Mutex _lock;

				// all bundles have to remain in this set to avoid duplicate delivery
				dtn::data::BundleSet _recv_bundles;

				// queue where the currently queued bundles are stored
				ibrcommon::Queue<dtn::data::MetaBundle> _queue;
			};

			const std::string _handle;
			dtn::data::EID _default_eid;

			ibrcommon::Mutex _endpoints_lock;
			std::set<dtn::data::EID> _endpoints;
			RegistrationQueue _queue;

			ibrcommon::Mutex _receive_lock;
			ibrcommon::Conditional _wait_for_cond;
			bool _no_more_bundles;

			ibrcommon::Queue<NOTIFY_CALL> _notify_queue;

			static const std::string gen_handle();
			static const std::string& alloc_handle();
			static const std::string& alloc_handle(const std::string &handle);
			static void free_handle(const std::string &handle);

			static ibrcommon::Mutex _handle_lock;
			static std::set<std::string> _handles;

			bool _persistent;
			bool _detached;
			ibrcommon::Mutex _attach_lock;
			ibrcommon::Timer::time_t _expiry;

			bool _filter_fragments;
		};
	}
}

#endif /* REGISTRATION_H_ */
