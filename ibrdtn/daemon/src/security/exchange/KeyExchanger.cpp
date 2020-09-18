/*
 * KeyExchanger.cpp
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

#include "config.h"
#include "security/exchange/KeyExchanger.h"
#include "security/SecurityKeyManager.h"

#include "core/BundleCore.h"
#include "core/EventDispatcher.h"
#include <ibrdtn/utils/Clock.h>

#include <ibrcommon/data/File.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/ssl/HMacStream.h>

#include <ibrdtn/security/SecurityKey.h>
#include <ibrdtn/utils/Random.h>

#include "security/exchange/NoneProtocol.h"
#include "security/exchange/DHProtocol.h"
#include "security/exchange/HashProtocol.h"
#include "security/exchange/QRCodeProtocol.h"
#include "security/exchange/NFCProtocol.h"

#ifdef HAVE_OPENSSL_JPAKE_H
#include "security/exchange/JPAKEProtocol.h"
#endif

#include <cstring>
#include <time.h>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdlib.h>

namespace dtn
{
	namespace security
	{
		const std::string KeyExchanger::TAG = "KeyExchanger";

		KeyExchanger::KeyExchanger()
		 : _next_expiration(0)
		{
			AbstractWorker::initialize("key-exchange");

			// add exchange protocols
			(new NoneProtocol(*this))->add(_protocols);
			(new DHProtocol(*this))->add(_protocols);
			(new HashProtocol(*this))->add(_protocols);
			(new QRCodeProtocol(*this))->add(_protocols);
			(new NFCProtocol(*this))->add(_protocols);

#ifdef HAVE_OPENSSL_JPAKE_H
			(new JPAKEProtocol(*this))->add(_protocols);
#endif
		}

		KeyExchanger::~KeyExchanger()
		{
			// wait until the main-thread is terminated
			join();

			// free all sessions
			for (std::map<std::string, KeyExchangeSession*>::iterator it = _sessionmap.begin(); it != _sessionmap.end(); ++it)
			{
				KeyExchangeSession *session = (*it).second;
				delete session;
			}
			_sessionmap.clear();
		}

		void KeyExchanger::__cancellation() noexcept
		{
			_queue.abort();
		}

		void KeyExchanger::componentUp() noexcept
		{
			dtn::core::EventDispatcher<dtn::security::KeyExchangeEvent>::add(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::add(this);

			// reset the queue
			_queue.reset();
		}

		void KeyExchanger::componentRun() noexcept
		{
			// initialize all protocols
			for (std::map<int, KeyExchangeProtocol*>::iterator it = _protocols.begin(); it != _protocols.end(); ++it)
			{
				KeyExchangeProtocol *p = (*it).second;
				p->initialize();
			}

			try
			{
				while (true)
				{
					Task *t = _queue.poll();
					std::auto_ptr<Task> killer(t);

					// execute the task
					t->execute(*this);

					ibrcommon::Thread::yield();
				}
			}
			catch (const ibrcommon::QueueUnblockedException &e)
			{
				// exit loop
			}
		}

		void KeyExchanger::componentDown() noexcept
		{
			dtn::core::EventDispatcher<dtn::security::KeyExchangeEvent>::remove(this);
			dtn::core::EventDispatcher<dtn::core::TimeEvent>::remove(this);
		}

		const std::string KeyExchanger::getName() const
		{
			return "KeyExchanger";
		}

		void KeyExchanger::callbackBundleReceived(const dtn::data::Bundle &b)
		{
			// do not receive own bundles
			if (b.source.sameHost(dtn::core::BundleCore::local)) return;

			try
			{
				// read payload block
				const dtn::data::PayloadBlock &p = b.find<dtn::data::PayloadBlock>();

				// read the data of the bundle -locked
				{
					ibrcommon::BLOB::Reference ref = p.getBLOB();
					ibrcommon::BLOB::iostream stream = ref.iostream();

					// read the data
					dtn::security::KeyExchangeData data;
					(*stream) >> data;

					// distribute the received message as event
					KeyExchangeEvent::raise(b.source, data);
				}

			} catch (const ibrcommon::Exception&) {}
		}

		void KeyExchanger::raiseEvent(const KeyExchangeEvent &kee) noexcept
		{
			_queue.push(new ExchangeTask(kee.getEID(), kee.getData()));
		}

		void KeyExchanger::raiseEvent(const dtn::core::TimeEvent&) noexcept
		{
			const dtn::data::Timestamp now = dtn::utils::Clock::getMonotonicTimestamp();

			ibrcommon::MutexLock l(_expiration_lock);
			if ((_next_expiration > 0) && (_next_expiration <= now))
			{
				// set next expiration to zero to prevent further expiration tasks
				_next_expiration = 0;

				// queue task to free expired sessions
				_queue.push(new ExpireTask(now));
			}
		}

		void KeyExchanger::submit(KeyExchangeSession &session, const KeyExchangeData &data)
		{
			dtn::data::Bundle b;

			try {
				ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

				// create the payload of the bundle
				{
					ibrcommon::BLOB::iostream stream = ref.iostream();

					// write the data
					(*stream) << data;
				}

				// add the payload to the message
				b.push_back(ref);

				// set the source
				b.source = dtn::core::BundleCore::local;

				// set source application
				b.source.setApplication("key-exchange");

				// set the destination
				b.destination = session.getPeer();

				// set destination application
				b.destination.setApplication("key-exchange");

				// set high priority
				b.set(dtn::data::PrimaryBlock::PRIORITY_BIT1, false);
				b.set(dtn::data::PrimaryBlock::PRIORITY_BIT2, true);

				// set the the destination as singleton receiver
				b.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);

				// set the lifetime of the bundle to 60 seconds
				b.lifetime = 300;

				transmit(b);

				// prevent expiration of the session
				session.touch();
			} catch (const ibrcommon::IOException &ex) {
				IBRCOMMON_LOGGER_TAG(KeyExchanger::TAG, error) << "error while key exchange, Exception: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void KeyExchanger::finish(KeyExchangeSession &session)
		{
			SecurityKey newKey;
			try
			{
				newKey = session.getKey(SecurityKey::KEY_PUBLIC);
			}
			catch (SecurityKey::KeyNotFoundException &e)
			{
				// This should never happen!!!
				throw ibrcommon::Exception("Error while reading temp key");
			}

			// prepare event
			KeyExchangeData event(KeyExchangeData::COMPLETE, session);

			try
			{
				// get the old stored key
				SecurityKey oldKey = SecurityKeyManager::getInstance().get(session.getPeer(), SecurityKey::KEY_PUBLIC);

				if (newKey == oldKey)
				{
					// merge flags
					newKey.flags |= oldKey.flags;

					// update existing key
					SecurityKeyManager::getInstance().store(newKey);

					event.setAction(KeyExchangeData::COMPLETE);
				}
				else
				{
					event.setAction(KeyExchangeData::NEWKEY_FOUND);

					// store new keys fingerprint
					event.str(newKey.getFingerprint());
				}
			}
			catch (SecurityKey::KeyNotFoundException &e)
			{
				// no old key available - store the new key
				SecurityKeyManager::getInstance().store(newKey);

				// store new keys fingerprint
				event.str(newKey.getFingerprint());
			}

			// trigger further actions
			KeyExchangeEvent::raise(session.getPeer(), event);

			if (event.getAction() == KeyExchangeData::COMPLETE) {
				// release the session
				freeSession(session.getPeer(), session.getUniqueId());
			}
		}

		void KeyExchanger::error(KeyExchangeSession &session, bool reportError)
		{
			// generate an error report
			KeyExchangeData error(KeyExchangeData::ERROR, session);
			KeyExchangeEvent::raise(session.getPeer(), error);
			if (reportError) submit(session, error);
		}

		KeyExchanger::Task::~Task()
		{
		}

		KeyExchanger::ExchangeTask::ExchangeTask(const dtn::data::EID &peer, const dtn::security::KeyExchangeData &data)
		 : _peer(peer), _data(data)
		{}

		void KeyExchanger::ExchangeTask::execute(KeyExchanger &exchanger) noexcept
		{
			try
			{
				// get the protocol instance
				std::map<int, KeyExchangeProtocol*>::iterator p_it;

				switch (_data.getProtocol())
				{
					case 100:
					{
						// get the session
						KeyExchangeSession &session = exchanger.getSession(_peer, _data);

						if (_data.getStep() == 0)
						{
							// finalize the session
							exchanger.error(session, false);
						}
						else
						{
							// finalize the session
							exchanger.finish(session);
						}
						return;
					}

					case 101:
					{
						// get the session
						KeyExchangeSession &session = exchanger.getSession(_peer, _data);

						if (_data.getStep() == 0)
						{
							// discard the new key stored in the session
						}
						else
						{
							// accept the new key stored in the session
							const SecurityKey key = session.getKey(SecurityKey::KEY_PUBLIC);

							// store temporary key as public key of the peer
							SecurityKeyManager::getInstance().store(key);

							// prepare event
							KeyExchangeData event(KeyExchangeData::COMPLETE, session);
							event.str(key.getFingerprint());

							// trigger further actions
							KeyExchangeEvent::raise(session.getPeer(), event);
						}

						// clean-up the session
						exchanger.freeSession(_peer, session.getUniqueId());
						return;
					}

					default:
					{
						// get the protocol instance
						p_it = exchanger._protocols.find(_data.getProtocol());
						break;
					}
				}

				if (p_it == exchanger._protocols.end())
				{
					// error - protocol not supported
					throw ibrcommon::Exception("key-exchange message for unsupported protocol received");
				}

				KeyExchangeProtocol &p = (*p_it->second);

				if (_data.getAction() == KeyExchangeData::ERROR)
				{
					// clean-up the session
					exchanger.freeSession(_peer, _data.getSessionId());
				}
				else if (_data.getAction() == KeyExchangeData::START)
				{
					// create a session
					KeyExchangeSession &session = exchanger.createSession(p, _peer);

					// assign session to data object
					_data.setSession(session);

					try {
						// start session
						p.begin(session, _data);
					} catch (ibrcommon::Exception &e) {
						// trigger error handling
						exchanger.error(session, false);

						throw;
					}
				}
				else if ((_data.getAction() == KeyExchangeData::REQUEST) && (_data.getStep() == 0))
				{
					// create a session
					KeyExchangeSession &session = exchanger.createSession(p, _peer, _data);

					// assign session to data object
					_data.setSession(session);

					try {
						// execute the next step of the session
						p.step(session, _data);
					} catch (ibrcommon::Exception &e) {
						// trigger error handling
						exchanger.error(session, true);

						throw;
					}
				}
				else if ((_data.getAction() == KeyExchangeData::REQUEST) || (_data.getAction() == KeyExchangeData::RESPONSE))
				{
					// get the session
					KeyExchangeSession &session = exchanger.getSession(_peer, _data);

					// assign session to data object
					_data.setSession(session);

					try {
						// execute the next step of the session
						p.step(session, _data);
					} catch (ibrcommon::Exception &e) {
						// trigger error handling
						exchanger.error(session, true);

						throw;
					}
				}
			}
			catch (ibrcommon::Exception &e)
			{
				IBRCOMMON_LOGGER_TAG(KeyExchanger::TAG, error) << e.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		KeyExchanger::ExpireTask::ExpireTask(const dtn::data::Timestamp timestamp)
		 : _timestamp(timestamp)
		{}

		void KeyExchanger::ExpireTask::execute(KeyExchanger &exchanger) noexcept
		{
			// free expired sessions
			exchanger.expire(_timestamp);
		}

		void KeyExchanger::expire(const dtn::data::Timestamp timestamp)
		{
			dtn::data::Timestamp next_expiration = 0;

			// free expired sessions
			for (std::map<std::string, KeyExchangeSession*>::iterator it = _sessionmap.begin(); it != _sessionmap.end(); ++it)
			{
				KeyExchangeSession &session = *(*it).second;

				if (session.getExpiration() <= timestamp)
				{
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 25) << "session expired for peer " << session.getPeer().getString() << ", ID: " <<  session.getUniqueId() << IBRCOMMON_LOGGER_ENDL;

					// expired - trigger an error
					// the error event will finally delete the session
					error(session, false);
				}
				else
				{
					if ((next_expiration == 0) || (next_expiration > session.getExpiration()))
					{
						next_expiration = session.getExpiration();
					}
				}
			}

			// set next expiration time-stamp
			ibrcommon::MutexLock l(_expiration_lock);
			_next_expiration = next_expiration;
		}

		KeyExchangeSession& KeyExchanger::createSession(KeyExchangeProtocol &p, const dtn::data::EID &peer, const dtn::security::KeyExchangeData &data)
		{
			KeyExchangeSession* session = p.createSession(peer, data.getSessionId());
			_sessionmap[session->getSessionKey()] = session;

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 20) << "key-exchange session created for " << peer.getString() << ", ID: " << session->getUniqueId() << IBRCOMMON_LOGGER_ENDL;

			// force expiration checking
			ibrcommon::MutexLock l(_expiration_lock);
			_next_expiration = 1;

			return *session;
		}

		KeyExchangeSession& KeyExchanger::createSession(KeyExchangeProtocol &p, const dtn::data::EID &peer)
		{
			KeyExchangeSession* session = p.createSession(peer, (unsigned int)dtn::utils::Random::gen_number());
			_sessionmap[session->getSessionKey()] = session;

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 20) << "key-exchange session created for " << peer.getString() << ", ID: " << session->getUniqueId() << IBRCOMMON_LOGGER_ENDL;

			// force expiration checking
			ibrcommon::MutexLock l(_expiration_lock);
			_next_expiration = 1;

			return *session;
		}

		KeyExchangeSession& KeyExchanger::getSession(const dtn::data::EID &peer, const dtn::security::KeyExchangeData &data) noexcept (false)
		{
			const std::string session_key = KeyExchangeSession::getSessionKey(peer, data.getSessionId());

			std::map<std::string, KeyExchangeSession*>::iterator it = _sessionmap.find(session_key);
			if (it == _sessionmap.end())
			{
				std::stringstream ss;
				ss << "Session " << data.getSessionId() << " for " << peer.getString() << " not found";
				throw ibrcommon::Exception(ss.str());
			}
			return *(*it).second;
		}

		void KeyExchanger::freeSession(const dtn::data::EID &peer, const unsigned int uniqueId)
		{
			const std::string session_key = KeyExchangeSession::getSessionKey(peer, uniqueId);

			std::map<std::string, KeyExchangeSession*>::iterator it = _sessionmap.find(session_key);
			if (it == _sessionmap.end())
			{
				// this should never happen
				IBRCOMMON_LOGGER_DEBUG_TAG(KeyExchanger::TAG, 25) << "Session not found for " << peer.getString() << ", ID: " <<  uniqueId << IBRCOMMON_LOGGER_ENDL;
				return;
			}

			delete it->second;
			_sessionmap.erase(it);

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 25) << "session free'd for " << peer.getString() << ", ID: " <<  uniqueId << IBRCOMMON_LOGGER_ENDL;
		}
	} /* namespace security */
} /* namespace dtn */
