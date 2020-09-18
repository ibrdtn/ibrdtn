/*
 * KeyExchangeSession.h
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

#ifndef KEYEXCHANGESESSION_H_
#define KEYEXCHANGESESSION_H_

#include "config.h"
#include <ibrdtn/data/EID.h>
#include <ibrdtn/security/SecurityKey.h>
#include <vector>
#include <string>
#include <typeinfo>

namespace dtn
{
	namespace security
	{
		class KeyExchangeSession
		{
				static const std::string TAG;

			public:
				class SessionState
				{
				public:
					virtual ~SessionState() = 0;
				};

				KeyExchangeSession(int protocol, const dtn::data::EID &peer, unsigned int uniqueId, SessionState *state = NULL);
				virtual ~KeyExchangeSession();

				/**
				 * Returns the expiration time of this session
				 */
				dtn::data::Timestamp getExpiration() const;

				/**
				 * Prolong the lifetime of this session
				 */
				void touch();

				/**
				 * Returns the peer of the exchange session
				 */
				const dtn::data::EID& getPeer() const;

				/**
				 * Returns the protocol ID of this session
				 */
				int getProtocol() const;

				/**
				 * Returns the unique ID of the session
				 */
				unsigned int getUniqueId() const;

				/**
				 * Returns the session key
				 */
				const std::string& getSessionKey() const;

				/**
				 * Generates a session key based on the peer and the unique ID
				 */
				static std::string getSessionKey(const dtn::data::EID &peer, unsigned int uniqueId);

				/**
				 * Return the key stored within this session
				 */
				dtn::security::SecurityKey getKey(const dtn::security::SecurityKey::KeyType type = dtn::security::SecurityKey::KEY_UNSPEC) const noexcept (false);

				/**
				 * Stores a key in the session
				 */
				void putKey(const std::string &data, const dtn::security::SecurityKey::KeyType type, const dtn::security::SecurityKey::TrustLevel trust) const;

				/**
				 * Clear all stored key in the session
				 */
				void clearKeys() const;

				/**
				 * Returns the session state or throws an exception if the
				 * state does not match the requested approach
				 */
				template<class T>
				T& getState()
				{
					if (!_state) throw ibrcommon::Exception("no state found");

					try {
						return dynamic_cast<T&>(*_state);
					} catch (std::bad_cast&) {
						throw ibrcommon::Exception("State does not match the exchange protocol.");
					}
				}

			private:
				int _protocol;
				unsigned int _unique_id;
				dtn::data::EID _peer;
				std::string _session_key;

				// a state object which contains the state of the exchange approach
				SessionState *_state;

				dtn::data::Timestamp _expiration;
		};

	} /* namespace security */
} /* namespace dtn */
#endif /* KEYEXCHANGESESSION_H_ */
