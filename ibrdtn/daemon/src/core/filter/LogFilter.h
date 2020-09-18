/*
 * LogFilter.h
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
#include <ibrcommon/Logger.h>

#ifndef FILTER_LOGFILTER_H_
#define FILTER_LOGFILTER_H_

namespace dtn
{
	namespace core
	{
		class LogFilter : public BundleFilter
		{
		public:
			LogFilter(const ibrcommon::LogLevel::Level level, const std::string &msg);
			LogFilter(const int debug_level, const std::string &msg);
			virtual ~LogFilter();
			virtual ACTION evaluate(const FilterContext&) const noexcept;
			virtual ACTION filter(const FilterContext&, dtn::data::Bundle&) const noexcept;

		private:
			void log(const FilterContext &context) const noexcept;

			const static std::string TAG;
			const ibrcommon::LogLevel::Level _level;
			const std::string _msg;
			const int _debug_level;
		};
	} /* namespace core */
} /* namespace dtn */

#endif /* FILTER_LOGFILTER_H_ */
