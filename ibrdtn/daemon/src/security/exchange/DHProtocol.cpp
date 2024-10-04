/*
 * DHProtocol.cpp
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

#include "security/exchange/DHProtocol.h"
#include "security/SecurityKeyManager.h"
#include "core/BundleCore.h"
#include <ibrdtn/data/BundleString.h>

#include <ibrcommon/ssl/HMacStream.h>
#include <ibrcommon/Logger.h>

#include <openssl/rand.h>
#include <openssl/pem.h>
#include "openssl_compat.h"

#define DH_KEY_LENGTH 1024

namespace dtn
{
	namespace security
	{
		const std::string DHProtocol::TAG = "DHProtocol";

		DHProtocol::DHState::DHState()
		 : dh(NULL)
		{
		}

		DHProtocol::DHState::~DHState()
		{
			if (dh) DH_free(dh);
		}

		DHProtocol::DHProtocol(KeyExchangeManager &manager)
		 : KeyExchangeProtocol(manager, 1), _dh_params(NULL), _auto_generate_params(false)
		{
			// check if auto generation of parameters is enabled
			_auto_generate_params = dtn::daemon::Configuration::getInstance().getSecurity().isGenerateDHParamsEnabled();
		}

		DHProtocol::~DHProtocol()
		{
			if (_dh_params) DH_free(_dh_params);
		}

		void DHProtocol::generate_params()
		{
			// if there are already parameters do not generate them again
			if (_dh_params != NULL) return;

			// check if DH params already exist
			if (_dh_params_file.exists())
			{
				// load DH params from file
				FILE *fd = fopen(_dh_params_file.getPath().c_str(), "r");
				if (fd != NULL)
				{
					_dh_params = PEM_read_DHparams(fd, NULL, NULL, NULL);
					fclose(fd);

					// finish here, if the parameters are loaded from file
					if (_dh_params != NULL) return;
				}
			}

			// only generate parameters if this feature is enabled
			if (!_auto_generate_params)
				throw ibrcommon::Exception("automatic generation of DH parameters disabled");

			struct timeval time;
			::gettimeofday(&time, NULL);

			// initialize random seed generator
			RAND_seed(&time, sizeof time);

			// create new params
			_dh_params = DH_new();

			IBRCOMMON_LOGGER_TAG(TAG, notice) << "Generate new DH parameters -- This may take a while!" << IBRCOMMON_LOGGER_ENDL;

			if (!DH_generate_parameters_ex(_dh_params, DH_KEY_LENGTH, DH_GENERATOR_2, NULL))
			{
				throw ibrcommon::Exception("Error while generating DH parameters");
			}

			// store params in the file
			FILE *fd = fopen(_dh_params_file.getPath().c_str(), "w");
			if (fd != NULL)
			{
				PEM_write_DHparams(fd, _dh_params);
				fclose(fd);
			}

			IBRCOMMON_LOGGER_TAG(TAG, notice) << "New DH parameters generated" << IBRCOMMON_LOGGER_ENDL;
		}

		void DHProtocol::initialize()
		{
			// set path for DH params
			_dh_params_file = SecurityKeyManager::getInstance().getFilePath("dh_params", "pem");

			try {
				// generate parameters
				generate_params();
			} catch (const ibrcommon::Exception&) {
				// generation failed
			}
		}

		KeyExchangeSession* DHProtocol::createSession(const dtn::data::EID &peer, unsigned int uniqueId)
		{
			return new KeyExchangeSession(getProtocol(), peer, uniqueId, new DHState());
		}

		void DHProtocol::begin(KeyExchangeSession &session, KeyExchangeData &data)
		{
			const BIGNUM *pub_key, *p, *g;
			// get session state
			DHState &state = session.getState<DHState>();

			// generate parameters
			generate_params();

			// copy DH params for new context
			state.dh = DHparams_dup(_dh_params);

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 25) << "Checking DH parameters" << IBRCOMMON_LOGGER_ENDL;

			int codes;
			if (!DH_check(state.dh, &codes))
			{
				throw ibrcommon::Exception("Error while checking DH parameters");
			}

			IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 25) << "Generate DH keys" << IBRCOMMON_LOGGER_ENDL;

			if (!DH_generate_key(state.dh))
			{
				throw ibrcommon::Exception("Error while generating DH key");
			}

			// prepare request
			KeyExchangeData request(KeyExchangeData::REQUEST, session);

			DH_get0_pqg(state.dh, &p, NULL, &g);
			DH_get0_key(state.dh, &pub_key, NULL);

			write(request, pub_key);
			write(request, p);
			write(request, g);

			manager.submit(session, request);
		}

		void DHProtocol::step(KeyExchangeSession &session, KeyExchangeData &data)
		{
			// get session state
			DHState &state = session.getState<DHState>();

			switch (data.getStep())
			{
				case 0:
				{
					if (data.getAction() == KeyExchangeData::REQUEST)
					{
						BIGNUM *p = BN_new();
						BIGNUM *g = BN_new();
						if (p == NULL || g == NULL)
						{
							BN_free(p);
							BN_free(g);
							throw ibrcommon::Exception("Error while allocating space for DH parameters");
						}

						BIGNUM* pub_key = BN_new();
						read(data, &pub_key);

						// create new params
						state.dh = DH_new();

						// read p and g paramter from message
						read(data, &p);
						read(data, &g);

						if (DH_set0_pqg(state.dh, p, NULL, g))
						{
							BN_free(p);
							BN_free(g);
							BN_free(pub_key);
							throw ibrcommon::Exception("Error while setting DH parameters");
						}

						int codes;
						if (!DH_check(state.dh, &codes))
						{
							throw ibrcommon::Exception("Error while checking DH parameters");
						}

						IBRCOMMON_LOGGER_DEBUG_TAG(TAG, 25) << "Generate DH keys" << IBRCOMMON_LOGGER_ENDL;

						if (!DH_generate_key(state.dh))
						{
							throw ibrcommon::Exception("Error while generating DH key");
						}

						unsigned char* secret;
						long int length = sizeof(unsigned char) * (DH_size(state.dh));
						if (NULL == (secret = (unsigned char*) OPENSSL_malloc(length)))
						{
							BN_free(pub_key);
							throw ibrcommon::Exception("Error while allocating space for secret key");
						}

						DH_compute_key(secret, pub_key, state.dh);

						state.secret.assign((const char*)secret, length);

						KeyExchangeData response(KeyExchangeData::RESPONSE, session);
						const BIGNUM *state_dh_pub_key;
						DH_get0_key(state.dh, &state_dh_pub_key, NULL);
						write(response, state_dh_pub_key);

						manager.submit(session, response);

						BN_free(pub_key);
					}
					else
					{
						BIGNUM* pub_key = BN_new();
						read(data, &pub_key);

						unsigned char* secret;
						long int length = sizeof(unsigned char) * (DH_size(state.dh));
						if(NULL == (secret = (unsigned char*) OPENSSL_malloc(length)))
						{
							BN_free(pub_key);
							throw ibrcommon::Exception("Error while allocating space for secret key");
						}

						DH_compute_key(secret, pub_key, state.dh);
						state.secret.assign((char*) secret, length);

						// get local public key
						const SecurityKey pkey = SecurityKeyManager::getInstance().get(dtn::core::BundleCore::local, SecurityKey::KEY_PUBLIC);

						ibrcommon::HMacStream hstream(secret, (int) length);
						hstream << pkey.getData() << std::flush;

						KeyExchangeData response(KeyExchangeData::REQUEST, session);
						response.setStep(1);

						const dtn::data::BundleString hmac(ibrcommon::HashStream::extract(hstream));
						response << hmac;

						const dtn::data::BundleString publicK(pkey.getData());
						response << publicK;

						manager.submit(session, response);

						BN_free(pub_key);
					}
					break;
				}

				case 1:
				{
					if (data.getAction() == KeyExchangeData::REQUEST)
					{
						dtn::data::BundleString hmac;
						data >> hmac;

						dtn::data::BundleString publicK;
						data >> publicK;

						ibrcommon::HMacStream hmacstream((const unsigned char*) state.secret.c_str(), (int) state.secret.size());
						hmacstream << (const std::string&)publicK << std::flush;

						if (hmac != ibrcommon::HMacStream::extract(hmacstream))
						{
							throw ibrcommon::Exception("Error while comparing hmac");
						}

						// write key in tmp file
						session.putKey(publicK, SecurityKey::KEY_PUBLIC, SecurityKey::LOW);

						// get local public key
						const SecurityKey pkey = SecurityKeyManager::getInstance().get(dtn::core::BundleCore::local, SecurityKey::KEY_PUBLIC);

						// create a HMAC from public key
						ibrcommon::HMacStream hstream((const unsigned char*) state.secret.c_str(), (int) state.secret.size());
						hstream << pkey.getData() << std::flush;

						// bundle erzeugen: EID, hmac/publicKey, response, round 1
						KeyExchangeData response(KeyExchangeData::RESPONSE, session);
						response.setStep(1);

						const dtn::data::BundleString local_hmac(ibrcommon::HashStream::extract(hstream));
						response << local_hmac;

						const dtn::data::BundleString local_publicK(pkey.getData());
						response << local_publicK;

						// send bundle to peer
						manager.submit(session, response);

						// finish the key-exchange
						manager.finish(session);
					}
					else
					{
						dtn::data::BundleString hmac;
						data >> hmac;

						dtn::data::BundleString publicK;
						data >> publicK;

						ibrcommon::HMacStream hstream((unsigned char*) state.secret.c_str(), (int) state.secret.size());
						hstream << publicK << std::flush;

						if (hmac != ibrcommon::HMacStream::extract(hstream))
						{
							throw ibrcommon::Exception("Error while comparing hmac");
						}

						// write key in tmp file
						session.putKey(publicK, SecurityKey::KEY_PUBLIC, SecurityKey::LOW);

						// finish the key-exchange
						manager.finish(session);
					}
					break;
				}
			}
		}

		void DHProtocol::write(std::ostream &stream, const BIGNUM* bn)
		{
			unsigned char* buf = new unsigned char[(BN_num_bits(bn) + 1) * sizeof(char)];
			dtn::data::Number length = BN_bn2bin(bn, (unsigned char*)buf);

			if (length > 0)
			{
				stream << length;
				stream.write((char*)buf, length.get<size_t>());
				delete[] buf;
			}
			else
			{
				delete[] buf;
				throw ibrcommon::Exception("Error while parsing BIGNUM to string");
			}
		}

		void DHProtocol::read(std::istream &stream, BIGNUM **bn)
		{
			dtn::data::Number length;
			stream >> length;

			unsigned char* buf = new unsigned char[length.get<size_t>()];
			stream.read((char*)buf, length.get<size_t>());
			*bn = BN_bin2bn((unsigned char*)buf, (int) length.get<size_t>(), NULL);
			delete[] buf;

			if (*bn == NULL)
			{
				throw ibrcommon::Exception("Error while parsing string to BIGNUM");
			}
		}
	} /* namespace security */
} /* namespace dtn */
