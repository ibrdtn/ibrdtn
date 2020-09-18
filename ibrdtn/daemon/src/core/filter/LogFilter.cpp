/*
 * LogFilter.cpp
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

#include "core/filter/LogFilter.h"

namespace dtn
{
	namespace core
	{
		const std::string LogFilter::TAG = "BundleFilter";

		LogFilter::LogFilter(const int debug_level, const std::string &msg)
		 : _level(ibrcommon::LogLevel::debug), _msg(msg), _debug_level(debug_level)
		{
		}

		LogFilter::LogFilter(const ibrcommon::LogLevel::Level level, const std::string &msg)
		 : _level(level), _msg(msg), _debug_level(1)
		{
		}

		LogFilter::~LogFilter()
		{
		}

		void LogFilter::log(const FilterContext &context) const noexcept
		{
			try {
				switch (_level)
				{
				case ibrcommon::LogLevel::emergency:
					IBRCOMMON_LOGGER_TAG(TAG, emergency) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;

				case ibrcommon::LogLevel::alert:
					IBRCOMMON_LOGGER_TAG(TAG, alert) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;

				case ibrcommon::LogLevel::critical:
					IBRCOMMON_LOGGER_TAG(TAG, critical) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;

				case ibrcommon::LogLevel::error:
					IBRCOMMON_LOGGER_TAG(TAG, error) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;

				case ibrcommon::LogLevel::warning:
					IBRCOMMON_LOGGER_TAG(TAG, warning) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;

				case ibrcommon::LogLevel::notice:
					IBRCOMMON_LOGGER_TAG(TAG, notice) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;

				case ibrcommon::LogLevel::info:
					IBRCOMMON_LOGGER_TAG(TAG, info) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;

				case ibrcommon::LogLevel::debug:
					IBRCOMMON_LOGGER_DEBUG_TAG(TAG, _debug_level) << _msg << " (" << context.getBundleID().toString() << ")" << IBRCOMMON_LOGGER_ENDL;
					break;
				}
			} catch (const FilterException&) {

			}
		}

		BundleFilter::ACTION LogFilter::evaluate(const FilterContext &context) const noexcept
		{
			// print logging
			log(context);

			// forward call to the next filter or return with the default action
			return BundleFilter::evaluate(context);
		}

		BundleFilter::ACTION LogFilter::filter(const FilterContext &context, dtn::data::Bundle &bundle) const noexcept
		{
			// print logging
			log(context);

			// forward call to the next filter or return with the default action
			return BundleFilter::filter(context, bundle);
		}
	} /* namespace core */
} /* namespace dtn */
