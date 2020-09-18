/*
 * StatusReportGenerator.cpp
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

#include "core/EventDispatcher.h"
#include "core/StatusReportGenerator.h"
#include "core/BundleCore.h"
#include <ibrdtn/data/MetaBundle.h>

namespace dtn
{
	namespace core
	{
		using namespace dtn::data;

		StatusReportGenerator::StatusReportGenerator()
		{
			dtn::core::EventDispatcher<BundleEvent>::add(this);
		}

		StatusReportGenerator::~StatusReportGenerator()
		{
			dtn::core::EventDispatcher<BundleEvent>::remove(this);
		}

		void StatusReportGenerator::createStatusReport(const dtn::data::MetaBundle &b, StatusReportBlock::TYPE type, StatusReportBlock::REASON_CODE reason)
		{
			// create a new bundle
			Bundle bundle;

			// create a new statusreport block
			StatusReportBlock report;

			bundle.set(dtn::data::PrimaryBlock::APPDATA_IS_ADMRECORD, true);

			// get the flags and set the status flag
			report.status |= static_cast<char>(type);

			// set the reason code
			report.reasoncode |= static_cast<char>(reason);

			switch (type)
			{
				case StatusReportBlock::RECEIPT_OF_BUNDLE:
					report.timeof_receipt.set();
				break;

				case StatusReportBlock::CUSTODY_ACCEPTANCE_OF_BUNDLE:
					report.timeof_custodyaccept.set();
				break;

				case StatusReportBlock::FORWARDING_OF_BUNDLE:
					report.timeof_forwarding.set();
				break;

				case StatusReportBlock::DELIVERY_OF_BUNDLE:
					report.timeof_delivery.set();
				break;

				case StatusReportBlock::DELETION_OF_BUNDLE:
					report.timeof_deletion.set();
				break;

				default:

				break;
			}

			// set source and destination
			bundle.source = dtn::core::BundleCore::local;
			bundle.set(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON, true);
			bundle.destination = b.reportto;

			// set lifetime to the origin bundle lifetime
			bundle.lifetime = b.lifetime;

			// sign the report if the reference was signed too
			if (b.get(dtn::data::PrimaryBlock::DTNSEC_STATUS_VERIFIED))
				bundle.set(dtn::data::PrimaryBlock::DTNSEC_REQUEST_SIGN, true);

			// set bundle parameter
			report.bundleid = b;

			dtn::data::PayloadBlock &payload = bundle.push_back<dtn::data::PayloadBlock>();
			report.write(payload);

			dtn::core::BundleCore::inject(dtn::core::BundleCore::local, bundle);
		}

		void StatusReportGenerator::raiseEvent(const dtn::core::BundleEvent &bundleevent) noexcept
		{
			const dtn::data::MetaBundle &b = bundleevent.getBundle();

			// do not generate status reports for other status reports or custody signals
			if (b.get(dtn::data::PrimaryBlock::APPDATA_IS_ADMRECORD)) return;

			switch (bundleevent.getAction())
			{
			case BUNDLE_RECEIVED:
				if ( b.get(Bundle::REQUEST_REPORT_OF_BUNDLE_RECEPTION))
				{
					createStatusReport(b, StatusReportBlock::RECEIPT_OF_BUNDLE, bundleevent.getReason());
				}
				break;
			case BUNDLE_DELETED:
				if ( b.get(Bundle::REQUEST_REPORT_OF_BUNDLE_DELETION))
				{
					createStatusReport(b, StatusReportBlock::DELETION_OF_BUNDLE, bundleevent.getReason());
				}
				break;

			case BUNDLE_FORWARDED:
				if ( b.get(Bundle::REQUEST_REPORT_OF_BUNDLE_FORWARDING))
				{
					createStatusReport(b, StatusReportBlock::FORWARDING_OF_BUNDLE, bundleevent.getReason());
				}
				break;

			case BUNDLE_DELIVERED:
				if ( b.get(Bundle::REQUEST_REPORT_OF_BUNDLE_DELIVERY))
				{
					createStatusReport(b, StatusReportBlock::DELIVERY_OF_BUNDLE, bundleevent.getReason());
				}
				break;

			case BUNDLE_CUSTODY_ACCEPTED:
				if ( b.get(Bundle::REQUEST_REPORT_OF_CUSTODY_ACCEPTANCE))
				{
					createStatusReport(b, StatusReportBlock::CUSTODY_ACCEPTANCE_OF_BUNDLE, bundleevent.getReason());
				}
				break;

			default:
				break;
			}
		}
	}
}
