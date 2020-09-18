/*
 * SecurityKeyManager.h
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

#ifndef SECURITYKEYMANAGER_H_
#define SECURITYKEYMANAGER_H_

#include "Configuration.h"
#include <ibrdtn/security/SecurityKey.h>
#include <ibrdtn/data/EID.h>
#include <ibrdtn/data/BundleString.h>
#include <ibrdtn/data/SDNV.h>
#include <ibrcommon/data/File.h>
#include <iostream>

namespace dtn
{
	namespace security
	{
		class SecurityKeyManager : public dtn::daemon::Configuration::OnChangeListener
		{
			static const std::string TAG;

		public:
			class PathNotFoundException : public ibrcommon::Exception
			{
			public:
				PathNotFoundException(std::string what = "No security path configured.") : ibrcommon::Exception(what)
				{};

				virtual ~PathNotFoundException() noexcept {};
			};

			static SecurityKeyManager& getInstance();

			virtual ~SecurityKeyManager();

			/**
			 * Listen for changes of the configuration
			 */
			virtual void onConfigurationChanged(const dtn::daemon::Configuration &conf) noexcept;

			/**
			 * Checks if a security key exists
			 */
			bool hasKey(const dtn::data::EID &ref, const dtn::security::SecurityKey::KeyType type = dtn::security::SecurityKey::KEY_UNSPEC) const;

			/**
			 * Get a security key from the standard key path
			 */
			dtn::security::SecurityKey get(const dtn::data::EID &ref, const dtn::security::SecurityKey::KeyType type = dtn::security::SecurityKey::KEY_UNSPEC) const noexcept (false);

			/**
			 * Get a security key from the prefixed path
			 */
			dtn::security::SecurityKey get(const std::string &prefix, const dtn::data::EID &ref, const dtn::security::SecurityKey::KeyType type = dtn::security::SecurityKey::KEY_UNSPEC) const noexcept (false);

			/**
			 * Store a security key in the standard key path
			 */
			void store(const dtn::security::SecurityKey &key, const std::string &data);

			/**
			 * Store a security key in the prefixed path
			 */
			void store(const std::string &prefix, const dtn::security::SecurityKey &key, const std::string &data);

			/**
			 * Store a security key object in the standard key path
			 */
			void store(const dtn::security::SecurityKey &key);

			/**
			 * Returns the prefixes path to the key of given type
			 */
			const ibrcommon::File getKeyFile(const std::string &prefix, const dtn::data::EID &peer, const dtn::security::SecurityKey::KeyType type = dtn::security::SecurityKey::KEY_UNSPEC) const;

			/**
			 * Returns the path to the key of given type
			 */
			const ibrcommon::File getKeyFile(const dtn::data::EID &peer, const dtn::security::SecurityKey::KeyType type = dtn::security::SecurityKey::KEY_UNSPEC) const;

			/**
			 * Returns the path to a security related file based on the keyword.
			 */
			const ibrcommon::File getFilePath(const std::string &keyword, const std::string &extension) const;

			/**
			 * Remove a security key
			 */
			void remove(const SecurityKey &key);

		private:
			SecurityKeyManager();

			/**
			 * Hash an EID into a string
			 */
			static const std::string hash(const dtn::data::EID &eid);

			/**
			 * Hash an string into a hex string
			 */
			static const std::string hash(const std::string &value);

			/**
			 * Create a fresh RSA key pair
			 */
			void createRSA(const dtn::data::EID &ref, const int bits = 2048);

			/**
			 * Load a security key
			 */
			void load(dtn::security::SecurityKey &key) const;

			ibrcommon::File _path;
			ibrcommon::File _ca;
			ibrcommon::File _key;
		};
	}
}

#endif /* SECURITYKEYMANAGER_H_ */
