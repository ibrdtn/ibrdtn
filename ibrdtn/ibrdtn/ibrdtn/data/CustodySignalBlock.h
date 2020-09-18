/*
 * CustodySignalBlock.h
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



#ifndef CUSTODYSIGNALBLOCK_H_
#define CUSTODYSIGNALBLOCK_H_

#include "ibrdtn/data/AdministrativeBlock.h"
#include "ibrdtn/data/EID.h"
#include "ibrdtn/data/Number.h"
#include "ibrdtn/data/MetaBundle.h"
#include "ibrcommon/data/BLOB.h"
#include "ibrdtn/data/DTNTime.h"

namespace dtn
{
	namespace data
	{
		class CustodySignalBlock : public AdministrativeBlock
		{
		public:
			enum REASON_CODE
			{
				NO_ADDITIONAL_INFORMATION = 0x00,
				RESERVED_01 = 0x01,
				RESERVED_02 = 0x02,
				REDUNDANT_RECEPTION = 0x03,
				DEPLETED_STORAGE = 0x04,
				DESTINATION_ENDPOINT_ID_UNINTELLIGIBLE = 0x05,
				NO_KNOWN_ROUTE_TO_DESTINATION_FROM_HERE = 0x06,
				NO_TIMELY_CONTACT_WITH_NEXT_NODE_ON_ROUTE = 0x07,
				BLOCK_UNINTELLIGIBLE = 0x08
			};

			CustodySignalBlock();
			virtual ~CustodySignalBlock();

			void setMatch(const dtn::data::MetaBundle& other);
			void setMatch(const dtn::data::Bundle& other);
			bool match(const dtn::data::Bundle& other) const;

			virtual void read(const dtn::data::PayloadBlock &p) noexcept (false);
			virtual void write(dtn::data::PayloadBlock &p) const;

			bool custody_accepted;
			REASON_CODE reason;
			DTNTime timeofsignal;
			dtn::data::BundleID bundleid;
		};
	}
}

#endif /* CUSTODYSIGNALBLOCK_H_ */
