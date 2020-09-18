/*
 * SecurityManager.h
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

#ifndef _SECURITY_MANAGER_H_
#define _SECURITY_MANAGER_H_

#include "Configuration.h"
#include <ibrdtn/data/EID.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/security/SecurityBlock.h>
#include <map>

namespace dtn
{
	namespace security
	{
		/**
		 * Decrypts or encrypts and signs or verifies bundles, which go in or out.
		 * The rules are read from the configuration and the keys needed for
		 * operation must be in the same directory as the configuration or be
		 * retrievable from the net.
		*/
		class SecurityManager
		{
			public:
				class KeyMissingException : public SecurityException
				{
				public:
					KeyMissingException(std::string what = "Key for this operation is not available.") : SecurityException(what)
					{};

					virtual ~KeyMissingException() noexcept {};
				};

				/**
				 * Returns a singleton instance of this class.
				 * @return a reference to this class singleton
				 */
				static SecurityManager& getInstance();

				/**
				 * This method signs the bundle with the own private key. If no key is
				 * available a KeyMissingException is thrown.
				 * @param bundle A bundle to sign.
				 */
				void sign(dtn::data::Bundle &bundle) const noexcept (false);
				void auth(dtn::data::Bundle &bundle) const noexcept (false);

				/**
				 * This method verifies the bundle and removes all auth or integrity block
				 * if they could validated.
				 * @param bundle The bundle to verify.
				 */
				void verifyAuthentication(dtn::data::Bundle &bundle) const noexcept (false);
				void verifyIntegrity(dtn::data::Bundle &bundle) const noexcept (false);

				/**
				 * This method decrypts encrypted payload of a bundle. It is necessary to
				 * remove all integrity or auth block before the payload can decrypted.
				 * @param bundle
				 */
				void decrypt(dtn::data::Bundle &bundle) const noexcept (false);

				/**
				 * This method encrypts the payload of a given bundle.
				 * If the bundle already contains integrity or auth block a EcryptException
				 * is thrown.
				 * @param bundle
				 */
				void encrypt(dtn::data::Bundle &bundle) const noexcept (false);

			protected:
				/**
				need a list of nodes, their security blocks type and the key
				for private and public keys
				*/
				SecurityManager();

				virtual ~SecurityManager();

			private:
				bool _accept_only_bab;
				bool _accept_only_pib;
		};
	}
}

#endif /* _SECURITY_MANAGER_H_ */
