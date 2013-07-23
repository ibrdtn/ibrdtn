/*
 * EMailSmtpService.cpp
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

#include "net/EMailSmtpService.h"
#include "net/EMailImapService.h"

#include "Configuration.h"
#include "core/BundleCore.h"
#include "core/BundleEvent.h"
#include "net/TransferAbortedEvent.h"
#include "net/TransferCompletedEvent.h"
#include "storage/BundleStorage.h"

#include <ibrcommon/Logger.h>
#include <vmime/platforms/posix/posixHandler.hpp>

namespace dtn
{
	namespace net
	{
		bool EMailSmtpService::_run(false);

		EMailSmtpService& EMailSmtpService::getInstance()
		{
			static EMailSmtpService instance;
			return instance;
		}

		EMailSmtpService::EMailSmtpService()
		 : _config(daemon::Configuration::getInstance().getEMail()),
		   _storage(dtn::core::BundleCore::getInstance().getStorage()),
		   _certificateVerifier(vmime::create<vmime::security::cert::defaultCertificateVerifier>())
		{
			// Load certificates
			loadCerificates();

			try {
				// Initialize VMime engine
				vmime::platform::setHandler<vmime::platforms::posix::posixHandler>();

				// Set SMTP server for outgoing mails (use SSL or not)
				std::string url;
				if(_config.smtpUseSSL())
				{
					url = "smtps://" + _config.getSmtpServer();
				}else{
					url = "smtp://" + _config.getSmtpServer();
				}
				vmime::ref<vmime::net::session> session =
						vmime::create<vmime::net::session>();

				// Create an instance of the transport service
				_transport = session->getTransport(vmime::utility::url(url));

				// Set username, password and port
				_transport->setProperty("options.need-authentication",
						_config.smtpAuthenticationNeeded());
				_transport->setProperty("auth.username", _config.getSmtpUsername());
				_transport->setProperty("auth.password", _config.getSmtpPassword());
				_transport->setProperty("server.port", _config.getSmtpPort());

				// Enable TLS
				if(_config.smtpUseTLS())
				{
					_transport->setProperty("connection.tls", true);
					_transport->setProperty("connection.tls.required", true);
				}

				// Handle timeouts
				_transport->setTimeoutHandlerFactory(vmime::create<TimeoutHandlerFactory>());

				// Add certificate verifier
				if(_config.imapUseTLS() || _config.imapUseSSL())
				{
					_transport->setCertificateVerifier(_certificateVerifier);
				}
			} catch (vmime::exception &e) {
				IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: Unable to create SMTP service: " << e.what() << IBRCOMMON_LOGGER_ENDL;
			}

			// Lock mutex
			_threadMutex.enter();

			// Start the thread
			_run = true;
			this->start();
		}

		EMailSmtpService::~EMailSmtpService()
		{
			this->stop();
			this->join();

			// Delete remaining tasks
			_queue.reset();
			while(!_queue.empty()) {
				Task *t = _queue.front();
				_queue.pop();
				delete t;
				t = NULL;
			}
			_queue.abort();
		}

		void EMailSmtpService::run() throw ()
		{
			while(true)
			{
				_threadMutex.enter();

				if(!_run)
					break;

				try {
					// Get Task
					Task *t = _queue.getnpop(true, _config.getSmtpKeepAliveTimeout());

					//Check if bundle is still in the storage
					try {
						_storage.get(t->getJob().getBundle());
					} catch (dtn::storage::NoBundleFoundException&) {
						// Destroy Task
						delete t;
						t = NULL;
						continue;
					}

					try {
						// Submit Task
						submit(t);
						EMailImapService::getInstance().storeProcessedTask(t);
					} catch(std::exception &e) {
						if(!_run)
							break;
						if(dynamic_cast<vmime::exceptions::authentication_error*>(&e) != NULL)
						{
							dtn::net::TransferAbortedEvent::raise(t->getNode().getEID(), t->getJob().getBundle(), dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
							IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: SMTP authentication error (username/password correct?)" << IBRCOMMON_LOGGER_ENDL;
						}
						else if(dynamic_cast<vmime::exceptions::connection_error*>(&e) != NULL)
						{
							dtn::net::TransferAbortedEvent::raise(t->getNode().getEID(), t->getJob().getBundle(), dtn::net::TransferAbortedEvent::REASON_CONNECTION_DOWN);
							IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: Unable to connect to the SMTP server" << IBRCOMMON_LOGGER_ENDL;
						}
						else
						{
							dtn::net::TransferAbortedEvent::raise(t->getNode().getEID(), t->getJob().getBundle(), dtn::net::TransferAbortedEvent::REASON_UNDEFINED);
							IBRCOMMON_LOGGER(error) << "EMail Convergence Layer: SMTP error: " << e.what() << IBRCOMMON_LOGGER_ENDL;
						}
						// Delete task
						delete t;
						t = NULL;

						_threadMutex.leave();
						continue;
					}
				} catch(ibrcommon::QueueUnblockedException&) {
					// Disconnect from server
					disconnect();
				}
			}
		}

		void EMailSmtpService::__cancellation() throw ()
		{
			_run = false;
			disconnect();
			_queue.abort();
			_threadMutex.leave();
		}

		void EMailSmtpService::connect()
		{
			if(!isConnected())
				_transport->connect();
		}

		bool EMailSmtpService::isConnected()
		{
			return _transport->isConnected();
		}

		void EMailSmtpService::disconnect()
		{
			if(isConnected())
				_transport->disconnect();
		}

		void EMailSmtpService::loadCerificates()
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

		vmime::ref<vmime::security::cert::X509Certificate> EMailSmtpService::loadCertificateFromFile(const std::string &path)
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

		void EMailSmtpService::queueTask(Task *t)
		{
			_queue.push(t);
		}

		void EMailSmtpService::submitNow(Task *t)
		{
			_queue.push(t);
			_threadMutex.leave();
		}

		void EMailSmtpService::submitQueue()
		{
			_threadMutex.leave();
		}

		void EMailSmtpService::submit(Task *t)
		{
			// Check connection
			if(!isConnected())
				connect();

			const dtn::data::Bundle bundle = _storage.get(t->getJob().getBundle());

			// Create new email
			vmime::ref<vmime::message> msg = vmime::create <vmime::message>();
			vmime::ref<vmime::header> header = msg->getHeader();
			vmime::ref<vmime::body> body = msg->getBody();

			// Create header
			vmime::headerFieldFactory* hfFactory =
					vmime::headerFieldFactory::getInstance();

			// From
			header->appendField(hfFactory->create(vmime::fields::FROM,
					_config.getOwnAddress()));
			// To
			header->appendField(hfFactory->create(vmime::fields::TO,
					t->getRecipient()));
			// Subject
			header->appendField(hfFactory->create(vmime::fields::SUBJECT,
					"Bundle for " + t->getNode().getEID().getString()));

			// Insert header for primary block
			header->appendField(hfFactory->create("Bundle-EMailCL-Version", "1"));
			header->appendField(hfFactory->create("Bundle-Flags",
					bundle.procflags.toString()));
			header->appendField(hfFactory->create("Bundle-Destination",
					bundle.destination.getString()));
			header->appendField(hfFactory->create("Bundle-Source",
					bundle.source.getString()));
			header->appendField(hfFactory->create("Bundle-Report-To",
					bundle.reportto.getString()));
			header->appendField(hfFactory->create("Bundle-Custodian",
					bundle.custodian.getString()));
			header->appendField(hfFactory->create("Bundle-Creation-Time",
					bundle.timestamp.toString()));
			header->appendField(hfFactory->create("Bundle-Sequence-Number",
					bundle.sequencenumber.toString()));
			header->appendField(hfFactory->create("Bundle-Lifetime",
					bundle.lifetime.toString()));
			if(bundle.get(dtn::data::Bundle::FRAGMENT))
			{
				header->appendField(hfFactory->create("Bundle-Fragment-Offset",
						bundle.fragmentoffset.toString()));
				header->appendField(hfFactory->create("Bundle-Total-Application-Data-Unit-Length",
						bundle.appdatalength.toString()));
			}

			// Count additional blocks
			unsigned int additionalBlocks = 0;

			// Set MIME-Header if we we need attachments
			if(bundle.size() > 0)
				header->appendField(hfFactory->create("MIME-Version","1.0"));

			for (dtn::data::Bundle::const_iterator iter = bundle.begin(); iter != bundle.end(); ++iter) {
				// Payload Block
				try {
					const dtn::data::PayloadBlock &payload =
							dynamic_cast<const dtn::data::PayloadBlock&>(**iter);

					header->appendField(hfFactory->create("Bundle-Payload-Flags",
							toString(getProcFlags(payload))));
					header->appendField(hfFactory->create("Bundle-Payload-Block-Length",
							toString(payload.getLength())));
					header->appendField(hfFactory->create("Bundle-Payload-Data-Name",
							"payload.data"));

					// Create payload attachment
					ibrcommon::BLOB::iostream data = payload.getBLOB().iostream();
					std::stringstream ss;
					ss << data->rdbuf();

					vmime::ref<vmime::utility::inputStream> dataStream =
							vmime::create<vmime::utility::inputStreamStringAdapter>(ss.str());

					vmime::ref<vmime::attachment> payloadAttachement =
							vmime::create<vmime::fileAttachment>(
									dataStream,
									vmime::word("payload.data"),
									vmime::mediaType("application/octet-stream")
									);
					vmime::attachmentHelper::addAttachment(msg, payloadAttachement);
					continue;
				} catch(const std::bad_cast&) {};

				// Add extension blocks
				try {
					const dtn::data::Block &block = (**iter);

					// Create extension attachment
					std::stringstream attachment;
					size_t length = 0;
					(**iter).serialize(attachment, length);

					vmime::ref<vmime::utility::inputStream> dataStream =
							vmime::create<vmime::utility::inputStreamStringAdapter>(attachment.str());

					vmime::ref<vmime::attachment> extensionAttachement =
							vmime::create<vmime::fileAttachment>(
									dataStream,
									vmime::word("block-" + toString(additionalBlocks) + ".data"),
									vmime::mediaType("application/octet-stream")
									);

					vmime::attachmentHelper::addAttachment(msg, extensionAttachement);

					// Get header of attachment
					vmime::ref<vmime::header> extensionHeader = msg->getBody()->getPartAt(msg->getBody()->getPartCount()-1)->getHeader();
					std::stringstream ss;
					ss << (int)block.getType();

					extensionHeader->appendField(hfFactory->create("Block-Type", ss.str()));

					extensionHeader->appendField(hfFactory->create("Block-Processing-Flags",
							toString(getProcFlags(block))));

					// Get EIDs
					if(block.get(dtn::data::Block::BLOCK_CONTAINS_EIDS))
					{
						const Block::eid_list &eids = block.getEIDList();
						for(Block::eid_list::const_iterator eidIt = eids.begin(); eidIt != eids.end(); eidIt++)
						{
							extensionHeader->appendField(hfFactory->create("Block-EID-Reference",
									(*eidIt).getString()));
						}
					}

					// Add filename to header
					header->appendField(hfFactory->create("Bundle-Additional-Block",
												"block-" + toString(additionalBlocks++) + ".data"));
					continue;
				} catch(const std::bad_cast&) {};

			}
			_transport->send(msg);
			dtn::net::TransferCompletedEvent::raise(t->getNode().getEID(), bundle);
			dtn::core::BundleEvent::raise(bundle, dtn::core::BUNDLE_FORWARDED);
			IBRCOMMON_LOGGER(info) << "EMail Convergence Layer: Bundle " << t->getJob().getBundle().toString() << " for node " << t->getNode().getEID().getString() << " submitted via smtp" << IBRCOMMON_LOGGER_ENDL;
		}

		unsigned int EMailSmtpService::getProcFlags(const dtn::data::Block &block)
		{
			unsigned int out = 0;

			if (block.get(dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT))
				out |= dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT;
			if (block.get(dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED))
				out |= dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED;
			if (block.get(dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED))
				out |= dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED;
			if (block.get(dtn::data::Block::LAST_BLOCK))
				out |= dtn::data::Block::LAST_BLOCK;
			if (block.get(dtn::data::Block::DISCARD_IF_NOT_PROCESSED))
				out |= dtn::data::Block::DISCARD_IF_NOT_PROCESSED;
			if (block.get(dtn::data::Block::FORWARDED_WITHOUT_PROCESSED))
				out |= dtn::data::Block::FORWARDED_WITHOUT_PROCESSED;
			if (block.get(dtn::data::Block::BLOCK_CONTAINS_EIDS))
				out |= dtn::data::Block::BLOCK_CONTAINS_EIDS;

			return out;
		}

		std::string EMailSmtpService::toString(int i)
		{
			std::stringstream ss;
			ss << i;
			return ss.str();
		}

		EMailSmtpService::Task::Task(const dtn::core::Node &node, const dtn::net::BundleTransfer &job, std::string recipient)
		 : _node(node), _job(job), _recipient(recipient), _timesChecked(0) {}

		EMailSmtpService::Task::~Task() {}

		const dtn::core::Node EMailSmtpService::Task::getNode()
		{
			return _node;
		}

		const dtn::net::BundleTransfer EMailSmtpService::Task::getJob()
		{
			return _job;
		}

		std::string EMailSmtpService::Task::getRecipient()
		{
			return _recipient;
		}

		bool EMailSmtpService::Task::checkForReturningMail()
		{
			return (!++_timesChecked > daemon::Configuration::getInstance().getEMail().getReturningMailChecks());
		}

		bool EMailSmtpService::TimeoutHandler::isTimeOut()
		{
			if(!_run)
				return true;

			return (getTime() >= last +
					daemon::Configuration::getInstance().getEMail().getSmtpConnectionTimeout());
		}

		void EMailSmtpService::TimeoutHandler::resetTimeOut()
		{
			last = getTime();
		}

		bool EMailSmtpService::TimeoutHandler::handleTimeOut()
		{
			// true = continue, false = cancel
			return false;
		}

	}
}
