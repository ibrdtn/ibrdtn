/*
 * SecurityManager.cpp
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

#include "security/SecurityManager.h"
#include "security/SecurityKeyManager.h"
#include "core/BundleCore.h"
#include "routing/QueueBundleEvent.h"
#include <ibrdtn/security/BundleAuthenticationBlock.h>
#include <ibrdtn/security/PayloadIntegrityBlock.h>
#include <ibrdtn/security/PayloadConfidentialBlock.h>
#include <ibrdtn/security/ExtensionSecurityBlock.h>
#include <ibrcommon/Logger.h>

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

namespace dtn
{
	namespace security
	{
		SecurityManager& SecurityManager::getInstance()
		{
			static SecurityManager sec_man;
			return sec_man;
		}

		SecurityManager::SecurityManager()
		: _accept_only_bab(false), _accept_only_pib(false)
		{
		}

		SecurityManager::~SecurityManager()
		{
		}

		void SecurityManager::auth(dtn::data::Bundle &bundle) const noexcept (false)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("SecurityManager", 10) << "auth bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

			try {
				// try to load the local key
				const SecurityKey key = SecurityKeyManager::getInstance().get(dtn::core::BundleCore::local, SecurityKey::KEY_SHARED);

				// sign the bundle with BABs
				dtn::security::BundleAuthenticationBlock::auth(bundle, key);
			} catch (const SecurityKey::KeyNotFoundException &ex) {
				throw KeyMissingException(ex.what());
			}
		}

		void SecurityManager::sign(dtn::data::Bundle &bundle) const noexcept (false)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("SecurityManager", 10) << "sign bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

			try {
				// try to load the local key
				const SecurityKey key = SecurityKeyManager::getInstance().get(dtn::core::BundleCore::local, SecurityKey::KEY_PRIVATE);

				// sign the bundle with PIB
				dtn::security::PayloadIntegrityBlock::sign(bundle, key, bundle.destination.getNode());
			} catch (const SecurityKey::KeyNotFoundException &ex) {
				throw KeyMissingException(ex.what());
			}
		}

		void SecurityManager::verifyIntegrity(dtn::data::Bundle &bundle) const noexcept (false)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("SecurityManager", 10) << "verify signed bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

			// iterate through all blocks
			for (dtn::data::Bundle::iterator it = bundle.begin(); it != bundle.end();)
			{
				const dtn::data::Block &block = (**it);

				if (block.getType() == dtn::security::PayloadConfidentialBlock::BLOCK_TYPE) {
					// payload after a PCB can not verified until the payload is decrypted
					break;
				}

				try {
					const dtn::security::PayloadIntegrityBlock& pib = dynamic_cast<const dtn::security::PayloadIntegrityBlock&>(block);

					const SecurityKey key = SecurityKeyManager::getInstance().get(pib.getSecuritySource(bundle), SecurityKey::KEY_PUBLIC);

					// try to verify the bundle with the key for the current PIB
					dtn::security::PayloadIntegrityBlock::verify(bundle, key);

					// if we are the security destination
					if (pib.isSecurityDestination(bundle, dtn::core::BundleCore::local)) {
						// remove the valid PIB
						bundle.erase(it++);
					} else {
						++it;
					}

					// set the verify bit, after verification
					bundle.set(dtn::data::PrimaryBlock::DTNSEC_STATUS_VERIFIED, true);

					IBRCOMMON_LOGGER_DEBUG_TAG("SecurityManager", 5) << "Bundle " << bundle.toString() << " successfully verified" << IBRCOMMON_LOGGER_ENDL;
					continue;
				} catch (const dtn::security::VerificationSkippedException&) {
					// un-set the verify bit
					bundle.set(dtn::data::PrimaryBlock::DTNSEC_STATUS_VERIFIED, false);
				} catch (const SecurityKey::KeyNotFoundException&) {
					// un-set the verify bit
					bundle.set(dtn::data::PrimaryBlock::DTNSEC_STATUS_VERIFIED, false);
				} catch (const std::bad_cast&) {
					// current block is not a PIB
				}

				++it;
			}
		}

		void SecurityManager::verifyAuthentication(dtn::data::Bundle &bundle) const noexcept (false)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("SecurityManager", 10) << "verify authenticated bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

			// iterate over all BABs of this bundle
			dtn::data::Bundle::find_iterator it(bundle.begin(), dtn::security::BundleAuthenticationBlock::BLOCK_TYPE);
			while (it.next(bundle.end()))
			{
				const dtn::security::BundleAuthenticationBlock& bab = dynamic_cast<const dtn::security::BundleAuthenticationBlock&>(**it);

				// look for the right BAB-factory
				const dtn::data::EID node = bab.getSecuritySource(bundle);

				try {
					// try to load the key of the BAB
					const SecurityKey key = SecurityKeyManager::getInstance().get(node, SecurityKey::KEY_SHARED);

					// verify the bundle
					dtn::security::BundleAuthenticationBlock::verify(bundle, key);

					// strip all BAB of this bundle
					dtn::security::BundleAuthenticationBlock::strip(bundle);

					// set the verify bit, after verification
					bundle.set(dtn::data::PrimaryBlock::DTNSEC_STATUS_AUTHENTICATED, true);

					// at least one BAB has been authenticated, we're done!
					break;
				} catch (const SecurityKey::KeyNotFoundException&) {
					// no key for this node found
				}
			}
		}

		void SecurityManager::decrypt(dtn::data::Bundle &bundle) const noexcept (false)
		{
			// check if the bundle has to be decrypted, return when not
			if (std::count(bundle.begin(), bundle.end(), dtn::security::PayloadConfidentialBlock::BLOCK_TYPE) <= 0) return;

			// decrypt
			try {
				IBRCOMMON_LOGGER_DEBUG_TAG("SecurityManager", 10) << "decrypt bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

				// get the encryption key
				dtn::security::SecurityKey key = SecurityKeyManager::getInstance().get(dtn::core::BundleCore::local, dtn::security::SecurityKey::KEY_PRIVATE);

				// encrypt the payload of the bundle
				dtn::security::PayloadConfidentialBlock::decrypt(bundle, key);

				bundle.set(dtn::data::PrimaryBlock::DTNSEC_STATUS_CONFIDENTIAL, true);
			} catch (const ibrcommon::Exception &ex) {
				throw DecryptException(ex.what());
			}
		}

		void SecurityManager::encrypt(dtn::data::Bundle &bundle) const noexcept (false)
		{
			try {
				IBRCOMMON_LOGGER_DEBUG_TAG("SecurityManager", 10) << "encrypt bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

				// get the encryption key
				dtn::security::SecurityKey key = SecurityKeyManager::getInstance().get(bundle.destination, dtn::security::SecurityKey::KEY_PUBLIC);

				// encrypt the payload of the bundle
				dtn::security::PayloadConfidentialBlock::encrypt(bundle, key, dtn::core::BundleCore::local);
			} catch (const ibrcommon::Exception &ex) {
				throw EncryptException(ex.what());
			}
		}
	}
}
