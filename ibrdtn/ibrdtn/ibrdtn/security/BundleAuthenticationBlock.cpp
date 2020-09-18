/*
 * BundleAuthenticationBlock.cpp
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

#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/security/BundleAuthenticationBlock.h"
#include "ibrdtn/security/StrictSerializer.h"
#include "ibrcommon/ssl/HMacStream.h"
#include <ibrcommon/Logger.h>
#include <cstring>
#include <set>
#include <algorithm>

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

namespace dtn
{
	namespace security
	{
		const dtn::data::block_t BundleAuthenticationBlock::BLOCK_TYPE = SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK;

		dtn::data::Block* BundleAuthenticationBlock::Factory::create()
		{
			return new BundleAuthenticationBlock();
		}

		BundleAuthenticationBlock::BundleAuthenticationBlock()
		 : SecurityBlock(BUNDLE_AUTHENTICATION_BLOCK, BAB_HMAC)
		{
		}

		BundleAuthenticationBlock::~BundleAuthenticationBlock()
		{
		}

		void BundleAuthenticationBlock::auth(dtn::data::Bundle &bundle, const dtn::security::SecurityKey &key)
		{
			BundleAuthenticationBlock& bab_begin = bundle.push_front<BundleAuthenticationBlock>();
			bab_begin.set(dtn::data::Block::DISCARD_IF_NOT_PROCESSED, true);

			// set security source
			if (!key.reference.sameHost(bundle.source)) bab_begin.setSecuritySource( key.reference );

			dtn::data::Number correlator = createCorrelatorValue(bundle);
			bab_begin.setCorrelator(correlator);
			bab_begin.setCiphersuiteId(BAB_HMAC);

			BundleAuthenticationBlock& bab_end = bundle.push_back<BundleAuthenticationBlock>();
			bab_end.set(dtn::data::Block::DISCARD_IF_NOT_PROCESSED, true);

			bab_end.setCorrelator(correlator);
			bab_end._ciphersuite_flags |= CONTAINS_SECURITY_RESULT;

			std::string sizehash_hash = calcMAC(bundle, key);
			bab_end._security_result.set(SecurityBlock::integrity_signature, sizehash_hash);
		}

		void BundleAuthenticationBlock::verify(const dtn::data::Bundle &bundle, const dtn::security::SecurityKey &key) noexcept (false)
		{
			// store the correlator of the verified BABs
			dtn::data::Number correlator;

			// verify the babs of the bundle
			verify(bundle, key, correlator);
		}

		void BundleAuthenticationBlock::strip(dtn::data::Bundle &bundle, const dtn::security::SecurityKey &key)
		{
			// store the correlator of the verified BABs
			dtn::data::Number correlator;

			// verify the babs of the bundle
			verify(bundle, key, correlator);

			// iterate over all BABs
			dtn::data::Bundle::find_iterator it(bundle.begin(), BundleAuthenticationBlock::BLOCK_TYPE);
			while (it.next(bundle.end()))
			{
				const BundleAuthenticationBlock &bab = dynamic_cast<const BundleAuthenticationBlock&>(**it);

				// if the correlator is already authenticated, then remove the BAB
				if ((bab._ciphersuite_flags & SecurityBlock::CONTAINS_CORRELATOR) && (bab._correlator == correlator))
				{
					bundle.erase(it);
				}
			}
		}

		void BundleAuthenticationBlock::strip(dtn::data::Bundle& bundle)
		{
			bundle.erase(std::remove(bundle.begin(), bundle.end(), BundleAuthenticationBlock::BLOCK_TYPE), bundle.end());
		}

		void BundleAuthenticationBlock::verify(const dtn::data::Bundle& bundle, const dtn::security::SecurityKey &key, dtn::data::Number &correlator) noexcept (false)
		{
			// get the blocks, with which the key should match
			std::set<dtn::data::Number> correlators;

			// calculate the MAC of this bundle
			std::string our_hash_string = calcMAC(bundle, key);

			dtn::data::Bundle::const_find_iterator it(bundle.begin(), BundleAuthenticationBlock::BLOCK_TYPE);
			while (it.next(bundle.end()))
			{
				const BundleAuthenticationBlock &bab = dynamic_cast<const BundleAuthenticationBlock&>(**it);

				// the bab contains a security result
				if (bab._ciphersuite_flags & CONTAINS_SECURITY_RESULT)
				{
					// is this correlator known?
					if (correlators.find(bab._correlator) == correlators.end()) continue;

					std::string bab_result = bab._security_result.get(SecurityBlock::integrity_signature);
					if (our_hash_string == bab_result)
					{
						// hash matched
						correlator = bab._correlator;
						return;
					}

					IBRCOMMON_LOGGER_DEBUG_TAG("BundleAuthenticationBlock", 15) << "security mac does not match" << IBRCOMMON_LOGGER_ENDL;
				}
				// bab contains no security result but a correlator
				else if (bab._ciphersuite_flags &  CONTAINS_CORRELATOR)
				{
					// currently we only support BAB_HMAC mechanism
					if (bab._ciphersuite_id != SecurityBlock::BAB_HMAC) continue;

					// skip this BAB if the security source do not match the key
					if (!bab.isSecuritySource(bundle, key.reference)) continue;

					// remember it for later check
					correlators.insert(bab._correlator);
				}
			}

			throw VerificationFailedException();
		}

		std::string BundleAuthenticationBlock::calcMAC(const dtn::data::Bundle& bundle, const dtn::security::SecurityKey &key, const bool with_correlator, const dtn::data::Number &correlator)
		{
			std::string hmac_key = key.getData();
			ibrcommon::HMacStream hms((const unsigned char*)hmac_key.c_str(), static_cast<int>(hmac_key.length()));
			dtn::security::StrictSerializer ss(hms, BUNDLE_AUTHENTICATION_BLOCK, with_correlator, correlator);
			(dtn::data::DefaultSerializer&)ss << bundle;
			hms << std::flush;

			return ibrcommon::HashStream::extract(hms);
		}

		dtn::data::Length BundleAuthenticationBlock::getSecurityResultSize() const
		{
			// TLV type
			dtn::data::Length size = 1;
			// length of value length
			size += dtn::data::Number(EVP_MD_size(EVP_sha1())).getLength();
			// length of value
			size += EVP_MD_size(EVP_sha1());
			return size;
		}

	}
}
