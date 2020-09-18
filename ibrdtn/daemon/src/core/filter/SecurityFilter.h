/*
 * SecurityFilter.h
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

#include "core/BundleFilter.h"

#ifndef FILTER_SECURITYFILTER_H_
#define FILTER_SECURITYFILTER_H_

namespace dtn
{
	namespace core
	{
		class SecurityFilter : public BundleFilter {
		public:
			enum MODE {
				APPLY_AUTH,
				APPLY_INTEGRITY,
				APPLY_CONFIDENTIALITY,
				VERIFY_AUTH,
				VERIFY_INTEGRITY,
				VERIFY_CONFIDENTIALITY,
				DECRYPT
			};

			SecurityFilter(MODE mode, BundleFilter::ACTION positive = BundleFilter::PASS, BundleFilter::ACTION negative = BundleFilter::PASS);
			virtual ~SecurityFilter();

			virtual ACTION evaluate(const FilterContext&) const noexcept;
			virtual ACTION filter(const FilterContext&, dtn::data::Bundle&) const noexcept;

		private:
			const MODE _mode;
			const BundleFilter::ACTION _positive_action;
			const BundleFilter::ACTION _negative_action;
		};
	} /* namespace core */
} /* namespace dtn */

#endif /* FILTER_SECURITYFILTER_H_ */
