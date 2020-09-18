/*
 * SecurityFilter.cpp
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <jm@m-network.de>
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

#include <ibrdtn/ibrdtn.h>
#include "SecurityFilter.h"

#ifdef IBRDTN_SUPPORT_BSP
#include "security/SecurityManager.h"
#include <ibrdtn/security/BundleAuthenticationBlock.h>
#include <ibrdtn/security/PayloadIntegrityBlock.h>
#include <ibrdtn/security/PayloadConfidentialBlock.h>
#endif

namespace dtn
{
	namespace core
	{
		SecurityFilter::SecurityFilter(MODE m, BundleFilter::ACTION positive, BundleFilter::ACTION negative)
		 : _mode(m), _positive_action(positive), _negative_action(negative)
		{
		}

		SecurityFilter::~SecurityFilter()
		{
		}

		BundleFilter::ACTION SecurityFilter::evaluate(const FilterContext &context) const noexcept
		{
#ifdef IBRDTN_SUPPORT_BSP
			switch (_mode)
			{
				default:
					break;

				case VERIFY_AUTH:
				{
					try {
						// extract bundle from context
						const dtn::data::Bundle &bundle = context.getBundle();

						// check if at least one BAB is present
						if (std::count(bundle.begin(), bundle.end(), dtn::security::BundleAuthenticationBlock::BLOCK_TYPE) > 0)
						{
							if (_positive_action != BundleFilter::PASS) return _positive_action;
						}
						else
						{
							if (_negative_action != BundleFilter::PASS) return _negative_action;
						}
					} catch (const FilterException&) {
						// necessary bundle object is not present - abort the chain
						return BundleFilter::PASS;
					}
					break;
				}

				case VERIFY_INTEGRITY:
				{
					try {
						// extract bundle from context
						const dtn::data::Bundle &bundle = context.getBundle();

						// check if at least one PIB is present
						if (std::count(bundle.begin(), bundle.end(), dtn::security::PayloadIntegrityBlock::BLOCK_TYPE) > 0)
						{
							if (_positive_action != BundleFilter::PASS) return _positive_action;
						}
						else
						{
							if (_negative_action != BundleFilter::PASS) return _negative_action;
						}
					} catch (const FilterException&) {
						// necessary bundle object is not present - abort the chain
						return BundleFilter::PASS;
					}
					break;
				}

				case VERIFY_CONFIDENTIALITY:
				{
					try {
						// extract bundle from context
						const dtn::data::Bundle &bundle = context.getBundle();

						// check if at least one PCB is present
						if (std::count(bundle.begin(), bundle.end(), dtn::security::PayloadConfidentialBlock::BLOCK_TYPE) > 0)
						{
							if (_positive_action != BundleFilter::PASS) return _positive_action;
						}
						else
						{
							if (_negative_action != BundleFilter::PASS) return _negative_action;
						}
					} catch (const FilterException&) {
						// necessary bundle object is not present - abort the chain
						return BundleFilter::PASS;
					}
					break;
				}
			}
#endif

			// forward call to the next filter or return with the default action
			return BundleFilter::evaluate(context);
		}

		BundleFilter::ACTION SecurityFilter::filter(const FilterContext &context, dtn::data::Bundle &bundle) const noexcept
		{
#ifdef IBRDTN_SUPPORT_BSP
			switch (_mode)
			{
				default:
					break;

				case VERIFY_AUTH:
				{
					try {
						// do verify and strip blocks
						dtn::security::SecurityManager::getInstance().verifyAuthentication(bundle);
						if (_positive_action != BundleFilter::PASS) return _positive_action;
					} catch (const dtn::security::VerificationFailedException&) {
						// necessary bundle object is not present - abort the chain
						if (_negative_action != BundleFilter::PASS) return _negative_action;
					}
					break;
				}

				case APPLY_AUTH:
				{
					try {
						// apply authentication
						dtn::security::SecurityManager::getInstance().auth(bundle);
						if (_positive_action != BundleFilter::PASS) return _positive_action;
					} catch (const dtn::security::SecurityManager::KeyMissingException&) {
						if (_negative_action != BundleFilter::PASS) return _negative_action;
					}
					break;
				}

				case VERIFY_INTEGRITY:
				{
					try {
						// do verify and strip blocks
						dtn::security::SecurityManager::getInstance().verifyIntegrity(bundle);
						if (_positive_action != BundleFilter::PASS) return _positive_action;
					} catch (const dtn::security::VerificationFailedException&) {
						// necessary bundle object is not present - abort the chain
						if (_negative_action != BundleFilter::PASS) return _negative_action;
					}
					break;
				}
			}
#endif

			// forward call to the next filter or return with the default action
			return BundleFilter::filter(context, bundle);
		}
	} /* namespace core */
} /* namespace dtn */
