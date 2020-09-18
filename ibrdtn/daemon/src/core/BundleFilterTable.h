/*
 * BundleFilterTable.h
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
#include <list>

#ifndef BUNDLEFILTERTABLE_H_
#define BUNDLEFILTERTABLE_H_

namespace dtn
{
	namespace core
	{
		class BundleFilterTable : public BundleFilter
		{
		public:
			BundleFilterTable();
			virtual ~BundleFilterTable();

			/**
			 * Append a BundleFilter at the end of the chain
			 */
			void append(BundleFilter *filter);

			/**
			 * Insert a BundleFilter before the element on the given position
			 */
			void insert(unsigned int position, BundleFilter *filter);

			/**
			 * Clear the whole table
			 */
			void clear();

			/**
			 * Evaluates a context and results in ACCEPT, REJECT, or DROP directive
			 */
			virtual ACTION evaluate(const FilterContext &context) const noexcept;

			/**
			 * Filters a bundle with a context. The bundle may be modified during
			 * the processing.
			 */
			virtual ACTION filter(const FilterContext &context, dtn::data::Bundle &bundle) const noexcept;

		private:
			typedef std::list<BundleFilter*> chain;
			chain _chain;
		};
	} /* namespace core */
} /* namespace dtn */

#endif /* BUNDLEFILTERTABLE_H_ */
