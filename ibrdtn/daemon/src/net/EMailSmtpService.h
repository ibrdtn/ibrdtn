/*
 * EMailSmtpService.h
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: Björn Gernert <mail@bjoern-gernert.de>
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

#ifndef EMAILSMTPSERVICE_H_
#define EMAILSMTPSERVICE_H_

#include "Configuration.h"
#include "core/Node.h"
#include "net/BundleTransfer.h"
#include "storage/BundleStorage.h"
#include "ibrdtn/data/BundleID.h"

#include <ibrcommon/thread/Thread.h>
#include <ibrcommon/thread/Queue.h>
#include <vmime/vmime.hpp>

namespace dtn
{
	namespace net
	{
		class EMailSmtpService : public ibrcommon::JoinableThread
		{
		public:
			/**
			 * The task object which encapsulates a submit task for the
			 * SMTP service
			 */
			class Task {
			public:
				/**
				 * The constructor
				 *
				 * @param node The destination node
				 * @param bid The bundle id
				 * @param recipient The email address of the recipient
				 */
				Task(const dtn::core::Node &node,
						const dtn::net::BundleTransfer &job,
						std::string recipient);

				/**
				 * Default destructor
				 */
				virtual ~Task();

				/**
				 * @return The destination node
				 */
				const dtn::core::Node getNode();

				/**
				 * @return The bundle id
				 */
				const dtn::net::BundleTransfer getJob();

				/**
				 * @return The email address of the recipient
				 */
				std::string getRecipient();

				/**
				 * @return True, if this task needs be be checked for returning
				 *         mails
				 */
				bool checkForReturningMail();

			private:

				/**
				 * Stores the node
				 */
				const dtn::core::Node& _node;

				/**
				 * Stores the bundle id
				 */
				const dtn::net::BundleTransfer _job;

				/**
				 * Stores the email address of the recipient
				 */
				std::string _recipient;

				/**
				 * Stores the number of checks for returning mails
				 */
				size_t _timesChecked;
			};

			/**
			 * Returns the instance of the SMTP service
			 */
			static EMailSmtpService& getInstance();

			/**
			 * Stores the given task in the submit queue
			 *
			 * @param t The task which will be stored in the submit queue
			 */
			void queueTask(Task *t);

			/**
			 * Submits the given task immediately
			 *
			 * @param t The task which will be transmitted  immediately
			 */
			void submitNow(Task *t);

			/**
			 * Submits all stored tasks
			 */
			void submitQueue();

			/**
			 * The run method of the thread
			 */
			void run() throw ();

			/**
			 * The cancellation method of the thread
			 */
			void __cancellation() throw ();

		private:
			/**
			 * The timeout handler for VMime
			 */
			class TimeoutHandler : public vmime::net::timeoutHandler
			{
				public:
					bool isTimeOut();
					void resetTimeOut();
					bool handleTimeOut();
				private:
					unsigned int getTime()
					{
						return vmime::platform::getHandler()->getUnixTime();
					}
					unsigned int last;
			};

			/**
			 * The timeout handler factory for VMime
			 */
			class TimeoutHandlerFactory : public vmime::net::timeoutHandlerFactory
			{
			public:
				vmime::ref<vmime::net::timeoutHandler> create()
				{
					return vmime::create<TimeoutHandler>();
				}
			};

			/**
			 * Default constuctor
			 */
			EMailSmtpService();

			/**
			 * Default destructor
			 */
			virtual ~EMailSmtpService();

			/**
			 * Stores the configuration of the EMail Convergence Layer
			 */
			const dtn::daemon::Configuration::EMail& _config;

			/**
			 * Stores the bundle storage of the daemon
			 */
			dtn::storage::BundleStorage& _storage;

			/**
			 * Stores the VMime default certificate verifier
			 */
			vmime::ref<vmime::security::cert::defaultCertificateVerifier> _certificateVerifier;

			/**
			 * Stores the VMime transport service
			 */
			vmime::ref<vmime::net::transport> _transport;

			/**
			 * Queue which stores the tasks
			 */
			ibrcommon::Queue<Task*> _queue;

			/**
			 * Mutex for the thread
			 */
			ibrcommon::Mutex _threadMutex;

			/**
			 * Stores the status of the service
			 */
			static bool _run;

			/**
			 * Loads the certificates
			 */
			void loadCerificates();

			/**
			 * Loads the given certificate. The certificate itself needs to be
			 * DER or PEM encoded
			 *
			 * @param path Path to the certificate
			 *
			 * @throws InvalidCertificate
			 */
			vmime::ref<vmime::security::cert::X509Certificate> loadCertificateFromFile(const std::string &path);

			/**
			 * Submits the given task
			 *
			 * @param The task which will be transmitted
			 */
			void submit(Task *t);

			/**
			 * If not already connected, the connection to the SMTP server
			 * will be established.
			 */
			void connect();

			/**
			 * If a connection to a SMTP server is currently established,
			 * the connection will be closed.
			 */
			void disconnect();

			/**
			 * Check if a connection to a SMTP server is established.
			 *
			 * @return Will return "true" if a connection to a SMTP server
			 *         is established
			 */
			bool isConnected();

			/**
			 * Returns the processing flags of the given block encoded as
			 * an unsigned integer.
			 *
			 * @param block The block with the processing flags to encode
			 *
			 * @return The integer representation of the processing flags
			 *         of the given block
			 */
			unsigned int getProcFlags(const dtn::data::Block &block);

			/**
			 * Converts an integer into a string.
			 *
			 * @param i The integer which will be converted to a string
			 *
			 * @return The string representation of the given integer
			 */
			std::string toString(int i);

			/**
			 * Exception for an invalid certificate.
			 */
			class InvalidCertificate : public ibrcommon::Exception {
			public:
				InvalidCertificate() {}
				InvalidCertificate(std::string msg) : Exception(msg) {}
			};

		};
	}
}

#endif /* EMAILSMTPSERVICE_H_ */
