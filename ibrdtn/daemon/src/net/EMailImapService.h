/*
 * EMailImapService.h
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

#ifndef EMAILIMAPSERVICE_H_
#define EMAILIMAPSERVICE_H_

#include "Configuration.h"
#include "net/EMailSmtpService.h"

#include <ibrcommon/thread/Thread.h>
#include <vmime/vmime.hpp>

namespace dtn
{
	namespace net
	{
		class EMailImapService : public ibrcommon::JoinableThread
		{
		public:

			/**
			 * Returns the instance of the IMAP service
			 */
			static EMailImapService& getInstance();

			/**
			 * The run method of the thread
			 */
			void run() throw ();

			/**
			 * The cancellation method of the thread
			 */
			void __cancellation() throw ();

			/**
			 * Stores the given tasks in a list. After each fetch this list will
			 * be run through and all tasks will be checked if their returning
			 * mail limit is reached. If the limit is reached the task will be
			 * deleted
			 *
			 * @param The task to be stored
			 */
			void storeProcessedTask(EMailSmtpService::Task *task);

			/**
			 * Queries the IMAP server for new mails
			 */
			void fetchMails();

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
			EMailImapService();

			/**
			 * Default destructor
			 */
			virtual ~EMailImapService();

			/**
			 * If not already connected, the connection to the IMAP server
			 * will be established.
			 */
			void connect();

			/**
			 * Check if a connection to an IMAP server is established.
			 *
			 * @return Will return "true" if a connection to an IMAP server
			 *         is established
			 */
			bool isConnected();

			/**
			 * If a connection to an IMAP server is currently established,
			 * the connection will be closed.
			 */
			void disconnect();

			/**
			 * Query the IMAP server for new mails. If "unseen" mails are
			 * detected, they will be decoded. Returning mails will be
			 * checked against the processed tasks list. If a mail was
			 * returned, the ??? for the specific bundle will be called.    //TODO
			 *
			 * If all mails are checked the connection to the IMAP server
			 * will be closed.
			 */
			void queryServer();

			/**
			 * Loads the certificates
			 */
			void loadCerificates();

			/**
			 * Tries to generate a bundle out of the given message.
			 * An InvalidConversion exception will be thrown if this is
			 * not possible. If the conversion was successful, the
			 * BundleReceivedEvent will be called.
			 *
			 * The message will be marked as "seen".
			 *
			 * If enabled the message will be marked as deleted and the
			 * working folder will be expunged.
			 *
			 * @param msg The message which will be decoded
			 *
			 * @throws InvalidConversion
			 */
			void generateBundle(vmime::ref<vmime::net::message> &msg);

			/**
			 * Checks the given VMime message for a bundle id within the body
			 * or the attachments. If a bundle id was found, the bundle will
			 * be re-queued
			 *
			 * @param msg The VMime message
			 *
			 * @throws BidNotFound
			 */
			void returningMailCheck(vmime::ref<vmime::net::message> &msg);

			/**
			 * Searches for a bundle id in the given message. If no bundle
			 * ib was found an InvalidConversion will be thrown
			 *
			 * @param message The message containing the bundle id
			 *
			 * @throws InvalidConversion
			 *
			 * @return The found bundle id
			 */
			dtn::data::BundleID extractBID(const std::string &message);

			/**
			 * Searches the string between the given search string and the CRLF
			 * '\r\n'
			 *
			 * @param search The search string
			 * @param s The complete string
			 *
			 * @throws StringNotFound
			 *
			 * @return String between the search string and '\r'n'
			 */
			std::string searchString(const std::string &search, const std::string &s);

			/**
			 * Sets the given encoded processing flags to the given block.
			 *
			 * @param block The block on which the processing flags will be
			 *              set
			 * @param flags The encoded processing flags
			 */
			void setProcFlags(dtn::data::Block &block, size_t &flags);

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
			 * Stores the configuration of the EMail Convergence Layer
			 */
			const dtn::daemon::Configuration::EMail& _config;

			/**
			 * Stores the status of the service
			 */
			static bool _run;

			/**
			 * The IMAP store
			 */
			vmime::ref<vmime::net::store> _store;

			/**
			 * Mutex for the thread
			 */
			ibrcommon::Mutex _threadMutex;

			/**
			 * Stores the VMime default certificate verifier
			 */
			vmime::ref<vmime::security::cert::defaultCertificateVerifier> _certificateVerifier;

			/**
			 * The path to the working folder
			 */
			vmime::net::folder::path _path;

			/**
			 * The working folder
			 */
			vmime::ref<vmime::net::folder> _folder;

			/**
			 * Stores the submitted tasks
			 */
			std::list<EMailSmtpService::Task*> _processedTasks;

			/**
			 * Mutex for the processed tasks list
			 */
			ibrcommon::Mutex _processedTasksMutex;

			/**
			 * Converts a string into an integer.
			 *
			 * @param s The string which will be converted to an integer
			 *
			 * @return The integer of the given string
			 *
			 * @throws InvalidConversion
			 */
			int toInt(std::string s);

			/**
			 * Converts an integer into a string.
			 *
			 * @param i The integer which will be converted to a string
			 *
			 * @return The string representation of the given integer
			 */
			std::string toString(int i);

			/**
			 * Adds the list of EIDs to the given attachment
			 *
			 * @param block The block with the list of EIDs
			 * @param attachment The attachment where the list of EIDs will
			 *                   be added
			 */
			void addEIDList(dtn::data::Block &block,
					vmime::utility::ref<const vmime::attachment> &attachment);

			/**
			 * Exception for an invalid certificate.
			 */
			class InvalidCertificate : public ibrcommon::Exception {
			public:
				InvalidCertificate() {}
				InvalidCertificate(std::string msg) : Exception(msg) {}
			};

			/**
			 * Exception for an invalid conversion.
			 */
			class InvalidConversion : public ibrcommon::Exception {
			public:
				InvalidConversion() {}
				InvalidConversion(std::string msg) : Exception(msg) {}
			};

			/**
			 * Exception if the string could not be found within the source
			 */
			class StringNotFound : public ibrcommon::Exception {
			public:
				StringNotFound() {}
				StringNotFound(std::string msg) : Exception(msg) {}
			};

			/**
			 * Exception if the string could not be found within the source
			 */
			class BidNotFound : public ibrcommon::Exception {
			public:
				BidNotFound() {}
				BidNotFound(std::string msg) : Exception(msg) {}
			};

			/**
			 * Exception for an IMAP error.
			 */
			class IMAPException : public ibrcommon::Exception {
			public:
				IMAPException() {}
				IMAPException(std::string msg) : Exception(msg) {}
			};
		};
	}
}

#endif /* EMAILIMAPSERVICE_H_ */
