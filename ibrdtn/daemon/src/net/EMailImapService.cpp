/*
 * EMailImapService.cpp
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

#include "net/EMailImapService.h"
#include "net/BundleReceivedEvent.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "ibrdtn/data/BundleBuilder.h"
#include "routing/RequeueBundleEvent.h"

#include <ibrcommon/Logger.h>
#include <vmime/platforms/posix/posixHandler.hpp>

namespace dtn
{
	namespace net
	{
		bool EMailImapService::_run(false);

		EMailImapService& EMailImapService::getInstance()
		{
			static EMailImapService instance;
			return instance;
		}

		EMailImapService::EMailImapService()
		 : _config(daemon::Configuration::getInstance().getEMail()),
		   _certificateVerifier(vmime::create<vmime::security::cert::defaultCertificateVerifier>())
		{
			// Load certificates
			loadCerificates();

			try {
				// Initialize vmime engine
				vmime::platform::setHandler<vmime::platforms::posix::posixHandler>();

				// Set IMAP-Server for outgoing emails (use SSL or not)
				std::string url;
				if(_config.imapUseSSL())
				{
					url = "imaps://" + _config.getImapServer();
				}else{
					url = "imap://" + _config.getImapServer();
				}
				vmime::ref<vmime::net::session> session =
						vmime::create<vmime::net::session>();

				// Create an instance of the store service
				_store = session->getStore(vmime::utility::url(url));

				// Set username, password and port
				_store->setProperty("auth.username", _config.getImapUsername());
				_store->setProperty("auth.password", _config.getImapPassword());
				_store->setProperty("server.port", _config.getImapPort());

				// Enable TLS
				if(_config.imapUseTLS())
				{
					_store->setProperty("connection.tls", true);
					_store->setProperty("connection.tls.required", true);
				}

				// Add certificate verifier
				if(_config.imapUseTLS() || _config.imapUseSSL())
				{
					_store->setCertificateVerifier(_certificateVerifier);
				}

				// Handle timeouts
				_store->setTimeoutHandlerFactory(vmime::create<TimeoutHandlerFactory>());

				// Create the folder path
				std::vector<std::string> folderPath = _config.getImapFolder();
				if(!folderPath.empty())
				{
					for(std::vector<std::string>::iterator it = folderPath.begin() ; it != folderPath.end(); ++it)
						_path /= vmime::net::folder::path::component(*it);
				}

				// Set mutex
				_threadMutex.enter();

				// Start thread
				_run = true;
				this->start();
			} catch (vmime::exception &e) {
				IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: Unable to create IMAP service: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		EMailImapService::~EMailImapService()
		{
			this->stop();
			this->join();

			// Delete remaining processed tasks
			{
				ibrcommon::Mutex l(_processedTasksMutex);
				while(!_processedTasks.empty())
				{
					EMailSmtpService::Task *t = _processedTasks.front();
					_processedTasks.pop_front();
					delete t;
					t = NULL;
				}
			}
		}

		void EMailImapService::loadCerificates()
		{
			std::vector<std::string> certPath;
			// Load CA certificates
			std::vector<vmime::ref <vmime::security::cert::X509Certificate> > ca;
			certPath = _config.getTlsCACerts();
			if(!certPath.empty()) {
				for(std::vector<std::string>::iterator it = certPath.begin() ; it != certPath.end(); ++it)
				{
					try {
						ca.push_back(loadCertificateFromFile((*it)));
					}catch(InvalidCertificate &e) {
						IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: Certificate load error: " << e.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}
				_certificateVerifier->setX509RootCAs(ca);
			}

			// Load user certificates
			std::vector<vmime::ref <vmime::security::cert::X509Certificate> > user;
			certPath = _config.getTlsUserCerts();
			if(!certPath.empty()) {
				for(std::vector<std::string>::iterator it = certPath.begin() ; it != certPath.end(); ++it)
				{
					try {
						user.push_back(loadCertificateFromFile((*it)));
					}catch(InvalidCertificate &e) {
						IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: Certificate load error: " << e.what() << IBRCOMMON_LOGGER_ENDL;
					}
				}
				_certificateVerifier->setX509TrustedCerts(user);
			}
		}

		vmime::ref<vmime::security::cert::X509Certificate> EMailImapService::loadCertificateFromFile(const std::string &path)
		{
			std::ifstream certFile;
			certFile.open(path.c_str(), std::ios::in | std::ios::binary);
			if(!certFile)
			{
				throw(InvalidCertificate("Unable to find certificate at \"" + path + "\""));
				return NULL;
			}

			vmime::utility::inputStreamAdapter is(certFile);
			vmime::ref<vmime::security::cert::X509Certificate> cert;
			cert = vmime::security::cert::X509Certificate::import(is);
			if(cert != NULL)
			{
				return cert;
			}else{
				throw(InvalidCertificate("The certificate at \"" + path + "\" does not seem to be PEM or DER encoded"));
				return NULL;
			}
		}

		void EMailImapService::run() throw ()
		{
			while(true)
			{
				_threadMutex.enter();

				if(!_run)
					break;

				try {
					IBRCOMMON_LOGGER(info) << "EMail Convergence Layer: Looking for new bundles via IMAP" << IBRCOMMON_LOGGER_ENDL;
					queryServer();
				}catch(vmime::exception &e) {
					if(_run)
						IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: Error during IMAP fetch operation: " << e.what() << IBRCOMMON_LOGGER_ENDL;
				}catch(IMAPException &e) {
					if(_run)
						IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: IMAP error: " << e.what() << IBRCOMMON_LOGGER_ENDL;
				}
			}
		}

		void EMailImapService::__cancellation() throw ()
		{
			_run = false;
			disconnect();
			_threadMutex.leave();
		}

		void EMailImapService::storeProcessedTask(EMailSmtpService::Task *t)
		{
			ibrcommon::Mutex l(_processedTasksMutex);
			_processedTasks.push_back(t);
		}

		void EMailImapService::fetchMails() {
			if(_run)
				_threadMutex.leave();
		}

		bool EMailImapService::TimeoutHandler::isTimeOut()
		{
			if(!_run)
				return true;

			return (getTime() >= last +
					daemon::Configuration::getInstance().getEMail().getImapConnectionTimeout());
		}

		void EMailImapService::TimeoutHandler::resetTimeOut()
		{
			last = getTime();
		}

		bool EMailImapService::TimeoutHandler::handleTimeOut()
		{
			// true = continue, false = cancel
			return false;
		}

		void EMailImapService::connect()
		{
			// Connect to IMAP Server
			if(!isConnected())
			{
				try {
					_store->connect();
					// Get working folder
					if(!_path.isEmpty())
					{
						_folder = _store->getFolder(_path);
					} else {
						_folder = _store->getDefaultFolder();
					}
					// Open with read-write access
					_folder->open(vmime::net::folder::MODE_READ_WRITE, true);
				}catch(vmime::exceptions::certificate_verification_exception&) {
					throw IMAPException("Cannot verify certificate against trusted certificates");
				}catch(vmime::exceptions::authentication_error&) {
					throw IMAPException("Authentication error (username/password correct?)");
				}catch(vmime::exceptions::connection_error&) {
					throw IMAPException("Unable to connect to the IMAP server");
				}catch(vmime::exception &e) {
					throw IMAPException(e.what());
				}
			}
		}

		bool EMailImapService::isConnected()
		{
			return _store->isConnected();
		}

		void EMailImapService::disconnect()
		{
			if(isConnected())
			{
				if(_folder->isOpen())
					_folder->close(_config.imapPurgeMail());
				_store->disconnect();
			}
		}

		void EMailImapService::queryServer()
		{
			// Check connection
			if(!isConnected())
				connect();

			// Get all unread messages
			std::vector<vmime::ref<vmime::net::message> > allMessages;
			try {
				allMessages = _folder->getMessages();
				_folder->fetchMessages(allMessages,
					vmime::net::folder::FETCH_FLAGS | vmime::net::folder::FETCH_FULL_HEADER | vmime::net::folder::FETCH_UID);
			}catch(vmime::exception&) {
				// No messages found (folder empty)
			}
			for(std::vector<vmime::ref<vmime::net::message> >::iterator it =
					allMessages.begin(); it != allMessages.end(); ++it)
			{
				const int flags = (*it).get()->getFlags();

				if(!flags & vmime::net::message::FLAG_SEEN) {
					// Looking for new bundles for me
					try {
						try {
							IBRCOMMON_LOGGER(info) << "EMail Convergence Layer: Generating bundle from mail (" << it->get()->getUniqueId().c_str() << ")" << IBRCOMMON_LOGGER_ENDL;
							generateBundle((*it));
						}catch(IMAPException &e){
							try {
								returningMailCheck((*it));
							}catch(BidNotFound&) {
								IBRCOMMON_LOGGER(warning) << "EMail Convergence Layer: " << e.what() << IBRCOMMON_LOGGER_ENDL;
							}
						}
					}catch(vmime::exceptions::no_such_field &e) {
						IBRCOMMON_LOGGER(warning) << "EMail Convergence Layer: Mail " << (*it)->getUniqueId() << " has no subject, skipping" << IBRCOMMON_LOGGER_ENDL;
					}

					// Set mail flag to 'seen'
					(*it)->setFlags(vmime::net::message::FLAG_SEEN, vmime::net::message::FLAG_MODE_SET);

					// Purge storage if enabled
					if(_config.imapPurgeMail())
					{
						(*it)->setFlags(vmime::net::message::FLAG_DELETED, vmime::net::message::FLAG_MODE_SET);
					}
				} else {
					continue;
				}
			}

			// Delete old tasks
			{
				ibrcommon::Mutex l(_processedTasksMutex);
				std::list<EMailSmtpService::Task*>::iterator it = _processedTasks.begin();
				while(it != _processedTasks.end())
				{
					if(!(*it)->checkForReturningMail()) {
						dtn::net::BundleTransfer job = (*it)->getJob();
						job.complete();
						delete *it;
						*it = NULL;
						it = _processedTasks.erase(it);
					}
				}
			}

			disconnect();
		}

		void EMailImapService::generateBundle(vmime::ref<vmime::net::message> &msg)
		{

			// Create new Bundle
			dtn::data::Bundle newBundle;

			// Clear all blocks
			newBundle.clear();

			// Get header of mail
			vmime::ref<const vmime::header> header = msg->getHeader();
			std::string mailID = " (ID: " + msg->getUniqueId() + ")";

			// Check EMailCL version
			try {
				int version = toInt(header->findField("Bundle-EMailCL-Version")->getValue()->generate());
				if(version != 1)
					throw IMAPException("Wrong EMailCL version found in mail" + mailID);
			}catch(vmime::exceptions::no_such_field&) {
				throw IMAPException("Field \"Bundle-EMailCL-Version\" not found in mail" + mailID);
			}catch(InvalidConversion&) {
				throw IMAPException("Field \"Bundle-EMailCL-Version\" wrong formatted in mail" + mailID);
			}

			dtn::data::EID source;
			size_t timestamp = 0, sequencenumber = 0, fragmentoffset = 0;

			try {
				newBundle.procflags = toInt(header->findField("Bundle-Flags")->getValue()->generate());
				newBundle.destination = header->findField("Bundle-Destination")->getValue()->generate();
				source = dtn::data::EID(header->findField("Bundle-Source")->getValue()->generate());
				newBundle.source = source;
				newBundle.reportto = header->findField("Bundle-Report-To")->getValue()->generate();
				newBundle.custodian = header->findField("Bundle-Custodian")->getValue()->generate();
				timestamp = toInt(header->findField("Bundle-Creation-Time")->getValue()->generate());
				newBundle.timestamp = timestamp;
				sequencenumber = toInt(header->findField("Bundle-Sequence-Number")->getValue()->generate());
				newBundle.sequencenumber = sequencenumber;
				newBundle.lifetime = toInt(header->findField("Bundle-Lifetime")->getValue()->generate());
			}catch(vmime::exceptions::no_such_field&) {
				throw IMAPException("Missing bundle headers in mail" + mailID);
			}catch(InvalidConversion&) {
				throw IMAPException("Wrong formatted bundle headers in mail" + mailID);
			}

			// If bundle is a fragment both fields need to be set
			try {
				fragmentoffset = toInt(header->findField("Bundle-Fragment-Offset")->getValue()->generate());
				newBundle.appdatalength = toInt(header->findField("Bundle-Total-Application-Data-Unit-Length")->getValue()->generate());
			}catch(vmime::exceptions::no_such_field&) {
				fragmentoffset = 0;
				newBundle.appdatalength = 0;
			}catch(InvalidConversion&) {
				throw IMAPException("Wrong formatted fragment offset in mail" + mailID);
			}
			newBundle.fragmentoffset = fragmentoffset;

			//Check if bundle is already in the storage
			try {
				dtn::data::BundleID bid(source, timestamp, sequencenumber, (fragmentoffset > 0), fragmentoffset);
				if(dtn::core::BundleCore::getInstance().getRouter().isKnown(bid))
					throw IMAPException("Bundle in mail" + mailID + " already processed");
			} catch (dtn::storage::NoBundleFoundException&) {
				// Go ahead
			}

			// Validate the primary block
			try {
				dtn::core::BundleCore::getInstance().validate((dtn::data::PrimaryBlock)newBundle);
			}catch(dtn::data::Validator::RejectedException&) {
				throw IMAPException("Bundle in mail" + mailID + " was rejected by validator");
			}

			// Get names of additional blocks
			std::vector<vmime::ref<vmime::headerField> > additionalBlocks =
					header.constCast<vmime::header>()->findAllFields("Bundle-Additional-Block");
			std::vector<std::string> additionalBlockNames;
			for(std::vector<vmime::ref<vmime::headerField> >::iterator it = additionalBlocks.begin();
					it != additionalBlocks.end(); ++it)
			{
				additionalBlockNames.push_back((*it)->getValue()->generate());
			}

			// Get all attachments
			std::vector<vmime::ref<const vmime::attachment> > attachments =
					vmime::attachmentHelper::findAttachmentsInMessage(msg->getParsedMessage());

			// Extract payload block info
			size_t payloadFlags;
			std::string payloadName;
			try {
				payloadFlags = toInt(header->findField("Bundle-Payload-Flags")->getValue()->generate());
				// Payload length will be calculated automatically
				//length = toInt(header->findField("Bundle-Payload-Block-Length")->getValue()->generate());
				payloadName = header->findField("Bundle-Payload-Data-Name")->getValue()->generate();
			}catch(vmime::exceptions::no_such_field&) {
				// No payload block there
			}catch(InvalidConversion&) {
				throw IMAPException("Wrong formatted payload flags in mail" + mailID);
			}

			// Create new bundle builder
			dtn::data::BundleBuilder builder(newBundle);

			// Download attachments
			for(std::vector<vmime::ref<const vmime::attachment> >::iterator it = attachments.begin(); it != attachments.end(); ++it)
			{
				if((*it)->getName().getBuffer() == payloadName && !payloadName.empty()) {
					dtn::data::PayloadBlock &pb = newBundle.push_back<dtn::data::PayloadBlock>();
					// Set data
					ibrcommon::BLOB::Reference ref = pb.getBLOB();
					ibrcommon::BLOB::iostream payloadData = ref.iostream();
					vmime::utility::outputStreamAdapter data((*payloadData));
					(*it)->getData()->extract(data);
					// Set flags
					setProcFlags(pb, payloadFlags);
					continue;
				}

				// If current attachment name is contained in additional attachment list
				if(std::find(additionalBlockNames.begin(), additionalBlockNames.end(), (*it)->getName().getBuffer()) != additionalBlockNames.end())
				{
					// Search for the block type
					block_t blockType;
					try {
						blockType = toInt((*it)->getHeader()->findField("Block-Type")->getValue()->generate());
					}catch(vmime::exceptions::no_such_field&) {
						throw IMAPException("Block type not found in attachment of mail" + mailID);
					}

					// Search for processing flags
					size_t flags;
					try {
						flags = toInt((*it)->getHeader()->findField("Block-Processing-Flags")->getValue()->generate());
					}catch(vmime::exceptions::no_such_field&) {
						throw IMAPException("Missing block processing flags in extension attachment of mail" + mailID);
					}catch(InvalidConversion&) {
						throw IMAPException("Wrong formatted processing flags in extension attachment of mail" + mailID);
					}

					// Create a block object
					dtn::data::Block &block = builder.insert(blockType, flags);

					// Add EIDs
					try {
						addEIDList(block, (*it));
					}catch(InvalidConversion&) {
						throw IMAPException("Wrong formatted EID list in extension attachment of mail" + mailID);
					}

					// Get payload of current block
					std::stringstream ss;
					vmime::utility::outputStreamAdapter data(ss);
					(*it)->getData()->extract(data);

					block.deserialize(ss, ss.str().length());
				}
			}

			// Validate whole bundle
			try {
				dtn::core::BundleCore::getInstance().validate(newBundle);
			}catch(dtn::data::Validator::RejectedException&) {
				throw IMAPException("Bundle in mail" + mailID + " was rejected by validator");
			}

			// Raise default bundle received event
			dtn::net::BundleReceivedEvent::raise(newBundle.source, newBundle, false);
		}

		void EMailImapService::returningMailCheck(vmime::ref<vmime::net::message> &msg)
		{
			dtn::data::BundleID bid;

			bool bidFound = false;

			std::string s;
			vmime::utility::outputStreamStringAdapter out(s);

			vmime::messageParser mp(msg->getParsedMessage());

			for(int i = 0; i < mp.getTextPartCount(); ++i)
			{
				s.clear();
				mp.getTextPartAt(i)->getText()->extract(out);

				try {
					bid = extractBID(s);
					bidFound = true;
					break;
				}catch(InvalidConversion&) {}
			}

			if(!bidFound)
			{
				for(int i = 0; i < mp.getAttachmentCount(); ++i)
				{
					s.clear();
					mp.getAttachmentAt(i)->getData()->extract(out);

					try {
						bid = extractBID(s);
						bidFound = true;
						break;
					}catch(InvalidConversion&) {}
				}
			}

			{
				ibrcommon::Mutex l(_processedTasksMutex);
				for(std::list<EMailSmtpService::Task*>::iterator iter = _processedTasks.begin(); iter != _processedTasks.end(); ++iter)
				{
					if((*iter)->getJob().getBundle() == bid)
					{
						dtn::routing::RequeueBundleEvent::raise((*iter)->getNode().getEID(), bid);
						_processedTasks.erase(iter);
						break;
					}
				}
			}

			if(!bidFound)
				throw BidNotFound("Found no bundle ID");
		}

		dtn::data::BundleID EMailImapService::extractBID(const std::string &message) {
			try {
				dtn::data::EID source(searchString("Bundle-Source: ", message));
				size_t timestamp = toInt(searchString("Bundle-Creation-Time: ", message));
				size_t sequenceNumber = toInt(searchString("Bundle-Sequence-Number: ", message));
				size_t fragmentOffset = 0;
				try {
					fragmentOffset = toInt(searchString("Bundle-Fragment-Offset: ", message));
				}catch(...){}

				return dtn::data::BundleID(source, timestamp, sequenceNumber, fragmentOffset > 0, fragmentOffset);
			}catch(...) {
				throw InvalidConversion("No bundle ID was found");
			}
		}

		std::string EMailImapService::searchString(const std::string &search, const std::string &s)
		{
			size_t start = 0, end = 0;
			start = s.find(search);
			if(start == std::string::npos)
				throw StringNotFound("Unable to find the string");
			start = start + search.length();

			end = s.find("\r\n", start);
			if(start == std::string::npos)
				throw StringNotFound("Unable to find the string");

			return s.substr(start, end-start);
		}

		int EMailImapService::toInt(std::string s)
		{
			std::stringstream ss(s);
			int ret;
			ss >> ret;
			if (ss.rdbuf()->in_avail() == 0) return ret;
			else throw InvalidConversion("Invalid integer " + s + ".");
		}

		std::string EMailImapService::toString(int i)
		{
			std::stringstream ss;
			ss << i;
			return ss.str();
		}

		void EMailImapService::addEIDList(dtn::data::Block &block, vmime::utility::ref<const vmime::attachment> &attachment)
		{
			std::vector<vmime::ref<vmime::headerField> > eidFiels =
					attachment->getHeader().constCast<vmime::header>()->findAllFields("Block-EID-Reference");
			try {
				for(std::vector<vmime::ref<vmime::headerField> >::iterator it = eidFiels.begin(); it != eidFiels.end(); ++it)
					block.addEID(dtn::data::EID((*it)->getValue()->generate()));
			}catch(vmime::exceptions::no_such_field&) {
				throw InvalidConversion("Invalid EID field.");
			}
		}

		void EMailImapService::setProcFlags(dtn::data::Block &block, size_t &flags)
		{
			if(flags & dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT)
				block.set(dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT, true);
			if(flags & dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED)
				block.set(dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED, true);
			if(flags & dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED)
				block.set(dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED, true);
			if(flags & dtn::data::Block::LAST_BLOCK)
				block.set(dtn::data::Block::LAST_BLOCK, true);
			if(flags & dtn::data::Block::DISCARD_IF_NOT_PROCESSED)
				block.set(dtn::data::Block::DISCARD_IF_NOT_PROCESSED, true);
			if(flags & dtn::data::Block::FORWARDED_WITHOUT_PROCESSED)
				block.set(dtn::data::Block::FORWARDED_WITHOUT_PROCESSED, true);
			if(flags & dtn::data::Block::BLOCK_CONTAINS_EIDS)
				block.set(dtn::data::Block::BLOCK_CONTAINS_EIDS, true);
		}
	}
}
