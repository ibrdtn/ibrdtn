/*
 * StatusReportGenerator.h
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

#ifndef STATUSREPORTGENERATOR_H_
#define STATUSREPORTGENERATOR_H_

#include "core/EventReceiver.h"
#include "ibrdtn/data/StatusReportBlock.h"
#include "ibrdtn/data/MetaBundle.h"

#include "core/BundleEvent.h"

namespace dtn
{
	namespace core
	{
		class StatusReportGenerator : public EventReceiver<dtn::core::BundleEvent>
		{
		public:
			StatusReportGenerator();
			virtual ~StatusReportGenerator();

			void raiseEvent(const dtn::core::BundleEvent &evt) noexcept;

		private:
			/**
			 * This method generates a status report.
			 * @param b The attributes of the given bundle where used to generate the status report.
			 * @param type Defines the type of the report.
			 * @param reason Give a additional reason.
			 * @return A bundle with a status report block.
			 */
			void createStatusReport(const dtn::data::MetaBundle &b, dtn::data::StatusReportBlock::TYPE type, dtn::data::StatusReportBlock::REASON_CODE reason = dtn::data::StatusReportBlock::NO_ADDITIONAL_INFORMATION);
		};
	}
}


#endif /* STATUSREPORTGENERATOR_H_ */
