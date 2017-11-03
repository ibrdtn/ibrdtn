/*
 * NativeSession.h
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

#ifndef NATIVESESSION_H_
#define NATIVESESSION_H_

#include "api/NativeSerializerCallback.h"
#include "api/Registration.h"
#include "core/EventReceiver.h"
#include "routing/QueueBundleEvent.h"
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/StatusReportBlock.h>
#include <ibrdtn/data/CustodySignalBlock.h>
#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/RWMutex.h>
#include <ibrcommon/thread/Mutex.h>
#include <ibrcommon/Exceptions.h>

namespace dtn
{
	namespace api
	{
		class NativeSessionException : public ibrcommon::Exception
		{
		public:
			NativeSessionException(std::string what = "An error happened.") throw() : ibrcommon::Exception(what)
			{
			};
		};

		class BundleNotFoundException : public ibrcommon::Exception
		{
		public:
			BundleNotFoundException(std::string what = "Bundle not found.") throw() : ibrcommon::Exception(what)
			{
			};
		};

		class NativeSessionCallback {
		public:
			/**
			 * virtual destructor
			 */
			virtual ~NativeSessionCallback() = 0;

			/**
			 * Notifies the session callback if a new bundles is queued
			 * in the registration. Then this bundle may loaded into the
			 * local register using the load() method.
			 */
			virtual void notifyBundle(const dtn::data::BundleID &id) throw () = 0;

			/**
			 * Notifies the session callback if a new status report has
			 * been received.
			 */
			virtual void notifyStatusReport(const dtn::data::EID &source, const dtn::data::StatusReportBlock &report) throw () = 0;

			/**
			 * Notifies the session callback if a new custody signal has
			 * been received.
			 */
			virtual void notifyCustodySignal(const dtn::data::EID &source, const dtn::data::CustodySignalBlock &custody) throw () = 0;
		};

		class NativeSession {
			static const std::string TAG;

		public:
			enum RegisterIndex {
				REG1 = 0,
				REG2 = 1
			};

			/**
			 * Constructor of the native session
			 * @param cb A callback object for notifications. May be NULL.
			 */
			NativeSession(NativeSessionCallback *session_cb, NativeSerializerCallback *serializer_cb);

			/**
			 * Constructor of the native session
			 * @param cb A callback object for notifications. May be NULL.
			 */
			NativeSession(NativeSessionCallback *session_cb, NativeSerializerCallback *serializer_cb, const std::string &handle);

			/**
			 * Destructor
			 */
			virtual ~NativeSession();

			/**
			 * Destroy this session and block until
			 * this process is done
			 */
			void destroy() throw ();

			/**
			 * Returns the node EID of this device
			 */
			const dtn::data::EID& getNodeEID() const throw ();

			/**
			 * Set the default application endpoint suffix of this
			 * registration
			 */
			void setEndpoint(const std::string &suffix) throw (NativeSessionException);

			/**
			 * Resets the default endpoint to the unique registration
			 * identifier
			 */
			void resetEndpoint() throw ();

			/**
			 * Add an application endpoint suffix to the registration
			 */
			void addEndpoint(const std::string &suffix) throw (NativeSessionException);

			/**
			 * Remove an application endpoint suffix from the registration
			 */
			void removeEndpoint(const std::string &suffix) throw (NativeSessionException);

			/**
			 * Add an endpoint identifier to the registration
			 * (commonly used for group endpoints)
			 */
			void addRegistration(const dtn::data::EID &eid) throw (NativeSessionException);

			/**
			 * Remove an endpoint identifier from the registration
			 */
			void removeRegistration(const dtn::data::EID &eid) throw (NativeSessionException);

			/**
			 * Removes all registrations and reset the default endpoint to the unique registration identifier
			 */
			void clearRegistration() throw ();

			/**
			 * Retrieve all registered endpoints
			 */
			std::vector<std::string> getSubscriptions() throw ();

			/**
			 * Loads the next bundle in the queue into the local
			 * register
			 */
			void next(RegisterIndex ri) throw (BundleNotFoundException);

			/**
			 * Load a bundle into the local register
			 */
			void load(RegisterIndex ri, const dtn::data::BundleID &id) throw (BundleNotFoundException);

			/**
			 * Return the bundle in the register using the given callback
			 */
			void get(RegisterIndex ri) throw ();

			/**
			 * Return the bundle skeleton in the register using the given callback
			 */
			void getInfo(RegisterIndex ri) throw ();

			/**
			 * Delete the bundle in the local register from the storage
			 */
			void free(RegisterIndex ri) throw (BundleNotFoundException);

			/**
			 * Clear the local register
			 */
			void clear(RegisterIndex ri) throw ();

			/**
			 * Mark the bundle with the given ID as delivered.
			 */
			void delivered(const dtn::data::BundleID &id) const throw (BundleNotFoundException);

			/**
			 * Send the bundle in the local register
			 */
			dtn::data::BundleID send(RegisterIndex ri) throw ();

			/**
			 * Copy the given bundle into the local register
			 */
			void put(RegisterIndex ri, const dtn::data::Bundle &b) throw ();

			/**
			 * Copy the PrimaryBlock into the local register
			 */
			void put(RegisterIndex ri, const dtn::data::PrimaryBlock &b) throw ();

			/**
			 * Write byte into the payload block of the bundle in the
			 * register. If there is no payload block, this method will
			 * append a new one at the end of all blocks.
			 * @param ri Index to tell which bundle register to use.
			 * @param buf Buffer to copy.
			 * @param len The number of bytes to copy.
			 * @param offset Start here to write.
			 */
			void write(RegisterIndex ri, const char *buf, const size_t len, const size_t offset = std::string::npos) throw ();

			/**
			 * Read max. <len> bytes from the payload block in the bundle. If there
			 * is no payload block available the read method will set len = 0 and return.
			 * Otherwise the len variable contains the number of bytes written into buf.
			 * @param ri Index to tell which bundle register to use.
			 * @param buf Buffer to put the data into.
			 * @param len The size of the buf array. After the call it contains the number of bytes in the buffer.
			 * @param offset Start here to read.
			 */
			void read(RegisterIndex ri, char *buf, size_t &len, const size_t offset = 0) throw ();

			/**
			 * Receives one bundle and returns. If there is no bundle this call will
			 * block until a bundle is available or destroy() has been called.
			 */
			void receive() throw (NativeSessionException);

			/**
			 * Return the handle of this session
			 */
			const std::string& getHandle() const;

		private:
			// local registration
			dtn::api::Registration _registration;

			class BundleReceiver : public dtn::core::EventReceiver<dtn::routing::QueueBundleEvent>
			{
			public:
				BundleReceiver(NativeSession &session);
				virtual ~BundleReceiver();

				void raiseEvent(const dtn::routing::QueueBundleEvent &evt) throw ();

			private:
				NativeSession &_session;
			} _receiver;

			/**
			 * Push out an notification to the native session callback.
			 */
			void fireNotificationBundle(const dtn::data::BundleID &id) throw ();

			/**
			 * Push out an notification to the native session callback.
			 */
			void fireNotificationAdministrativeRecord(const dtn::data::MetaBundle &bundle);

			/**
			 * Push out an notification to the native session callback.
			 */
			void fireNotificationStatusReport(const dtn::data::EID &source, const dtn::data::StatusReportBlock &report) throw ();

			/**
			 * Push out an notification to the native session callback.
			 */
			void fireNotificationCustodySignal(const dtn::data::EID &source, const dtn::data::CustodySignalBlock &custody) throw ();

			// callback
			ibrcommon::RWMutex _cb_mutex;
			NativeSessionCallback *_session_cb;
			NativeSerializerCallback *_serializer_cb;

			// default endpoint
			dtn::data::EID _endpoint;

			// local bundle register
			dtn::data::Bundle _bundle[2];

			// local bundle queue
			ibrcommon::Queue<dtn::data::BundleID> _bundle_queue;
		};
	} /* namespace net */
} /* namespace dtn */
#endif /* NATIVESESSION_H_ */
