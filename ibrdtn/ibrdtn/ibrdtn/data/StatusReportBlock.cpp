/*
 * StatusReportBlock.cpp
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

#include "ibrdtn/data/StatusReportBlock.h"
#include "ibrdtn/data/Number.h"
#include "ibrdtn/data/BundleString.h"
#include "ibrdtn/data/PayloadBlock.h"
#include <stdlib.h>
#include <sstream>

namespace dtn
{
	namespace data
	{
		StatusReportBlock::StatusReportBlock()
		 : status(0), reasoncode(0),
		 timeof_receipt(), timeof_custodyaccept(),
		 timeof_forwarding(), timeof_delivery(), timeof_deletion()
		{
		}

		StatusReportBlock::~StatusReportBlock()
		{
		}

		void StatusReportBlock::write(dtn::data::PayloadBlock &p) const
		{
			ibrcommon::BLOB::Reference r = p.getBLOB();
			ibrcommon::BLOB::iostream stream = r.iostream();

			// clear the whole data first
			stream.clear();

			char admfield = bundleid.isFragment() ? 17 : 16;

			(*stream).put(admfield);
			(*stream).put(status);
			(*stream).put(reasoncode);

			if ( bundleid.isFragment() )
			{
				(*stream) << bundleid.fragmentoffset;
				(*stream) << dtn::data::Number(bundleid.getPayloadLength());
			}

			if (status & RECEIPT_OF_BUNDLE)
				(*stream) << timeof_receipt;

			if (status & CUSTODY_ACCEPTANCE_OF_BUNDLE)
				(*stream) << timeof_custodyaccept;

			if (status & FORWARDING_OF_BUNDLE)
				(*stream) << timeof_forwarding;

			if (status & DELIVERY_OF_BUNDLE)
				(*stream) << timeof_delivery;

			if (status & DELETION_OF_BUNDLE)
				(*stream) << timeof_deletion;

			(*stream) << bundleid.timestamp;
			(*stream) << bundleid.sequencenumber;
			(*stream) << BundleString(bundleid.source.getString());
		}

		void StatusReportBlock::read(const dtn::data::PayloadBlock &p) noexcept (false)
		{
			ibrcommon::BLOB::Reference r = p.getBLOB();
			ibrcommon::BLOB::iostream stream = r.iostream();

			char admfield;
			(*stream).get(admfield);

			// check type field
			if ((admfield >> 4) != 1) throw WrongRecordException();

			(*stream).get(status);
			(*stream).get(reasoncode);

			if ( admfield & 0x01 )
			{
				bundleid.setFragment(true);

				(*stream) >> bundleid.fragmentoffset;

				dtn::data::Number tmp;
				(*stream) >> tmp;
				bundleid.setPayloadLength(tmp.get<dtn::data::Length>());
			}

			if (status & RECEIPT_OF_BUNDLE)
				(*stream) >> timeof_receipt;

			if (status & CUSTODY_ACCEPTANCE_OF_BUNDLE)
				(*stream) >> timeof_custodyaccept;

			if (status & FORWARDING_OF_BUNDLE)
				(*stream) >> timeof_forwarding;

			if (status & DELIVERY_OF_BUNDLE)
				(*stream) >> timeof_delivery;

			if (status & DELETION_OF_BUNDLE)
				(*stream) >> timeof_deletion;

			(*stream) >> bundleid.timestamp;
			(*stream) >> bundleid.sequencenumber;

			BundleString source;
			(*stream) >> source;
			bundleid.source = EID(source);
		}

	}
}
