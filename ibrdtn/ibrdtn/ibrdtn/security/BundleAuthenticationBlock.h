/*
 * BundleAuthenticationBlock.h
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

#ifndef BUNDLE_AUTHENTICATION_BLOCK_H_
#define BUNDLE_AUTHENTICATION_BLOCK_H_

#include "ibrdtn/security/SecurityBlock.h"
#include "ibrdtn/security/SecurityKey.h"
#include "ibrdtn/data/ExtensionBlock.h"
#include "ibrdtn/data/EID.h"
#include "ibrdtn/data/Bundle.h"

namespace dtn
{
	namespace security
	{
		/**
		Calculates the HMAC (Hashed Message Authentication Code) for P2P connections
		of security aware nodes. You can instantiate a factory of this class, which 
		will be given keys and EIDs of the corresponding nodes.\n
		You can use addMAC() to add BundleAuthenticationBlock pairs for each given 
		key to the bundle. If you have received a Bundle you can verify it by using 
		the method verify() and then remove all BundleAuthenticationBlocks by using
		removeAllBundleAuthenticationBlocks() from the bundle.
		*/
		class BundleAuthenticationBlock : public SecurityBlock
		{
			/**
			This class is allowed to call the parameterless contructor.
			*/
			friend class dtn::data::Bundle;
			public:
				class Factory : public dtn::data::ExtensionBlock::Factory
				{
				public:
					Factory() : dtn::data::ExtensionBlock::Factory(BundleAuthenticationBlock::BLOCK_TYPE) {};
					virtual ~Factory() {};
					virtual dtn::data::Block* create();
				};

				/** The block type of this class. */
				static const dtn::data::block_t BLOCK_TYPE;

				/**
				Deletes all keys, which were used for calculating the MACs
				*/
				virtual ~BundleAuthenticationBlock();

				/**
				 * authenticate a given bundle
				 * @param bundle
				 * @param key
				 */
				static void auth(dtn::data::Bundle &bundle, const dtn::security::SecurityKey &key);

				/**
				 * Tests if the bundles MAC is correct. There might be multiple BABs inside
				 * the bundle, which may be tested.
				 * None of these BABs will be removed.
				 * @param bundle
				 * @param key
				 */
				static void verify(const dtn::data::Bundle &bundle, const dtn::security::SecurityKey &key) noexcept (false);

				/**
				 * strips verified BABs off the bundle
				 * @param bundle the bundle, which shall be cleaned from babs
				 * @param key
				 */
				static void strip(dtn::data::Bundle &bundle, const dtn::security::SecurityKey &key);

				/**
				 * strip all BABs off the bundle
				 * @param bundle the bundle, which shall be cleaned from babs
				 */
				static void strip(dtn::data::Bundle &bundle);

			protected:
				/**
				Creates an empty BundleAuthenticationBlock. This BAB is meant to be 
				inserted into a bundle, by a factory. Because the instantiation will be
				done by the bundle instance for memory management, this method will be
				called be the bundle. The ciphersuite id is set to BAB_HMAC.
				*/
				BundleAuthenticationBlock();

				/**
				Creates the MAC of a given bundle using the BAB_HMAC algorithm. If a 
				correlator is given the MAC is created for the primary block and the 
				part of the bundle between the two BABs with the correlator.
				@param bundle bundle of which the MAC shall be calculated
				@param key the key to be used for creating the MAC
				@param key_size the size of the key
				@param with_correlator tells if a correlator shall be used
				@param correlator the correlator which shall be used
				@return a string containing the MAC
				*/
				static std::string calcMAC(const dtn::data::Bundle& bundle, const dtn::security::SecurityKey &key, const bool with_correlator = false, const dtn::data::Number &correlator = 0);

				/**
				Tries to verify the bundle using the given key. If a BAB-pair is found,
				which contains a valid hash corresponding to the key, the first value of
				the returned pair is true and the second contains the correlator.
				otherwise the first value is false and the second undefined.
				@param bundle bundle which shall be verified
				@param key the key for testing
				@return first is true if the key matched and second is the correlator of
				the matching pair. otherwise the first is false, if there was no
				matching
				*/
				static void verify(const dtn::data::Bundle& bundle, const dtn::security::SecurityKey &key, dtn::data::Number &correlator) noexcept (false);

				/**
				Returns the size of the security result field. This is used for strict 
				canonicalisation, where the block itself is included to the canonical 
				form, but the security result is excluded or unknown.
				*/
				virtual dtn::data::Length getSecurityResultSize() const;
		};

		/**
		 * This creates a static block factory
		 */
		static BundleAuthenticationBlock::Factory __BundleAuthenticationBlockFactory__;
	}
}

#endif /* BUNDLE_AUTHENTICATION_BLOCK_H_ */
