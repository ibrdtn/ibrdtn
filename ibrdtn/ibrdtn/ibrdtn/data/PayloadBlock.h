/*
 * PayloadBlock.h
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



#ifndef PAYLOADBLOCK_H_
#define PAYLOADBLOCK_H_

#include "ibrdtn/data/Number.h"
#include "ibrdtn/data/Block.h"
#include "ibrcommon/data/BLOB.h"
#include "ibrdtn/data/BlobBlock.h"

namespace dtn
{
	namespace data
	{
		class PayloadBlock : public BlobBlock
		{
		public:
			static const dtn::data::block_t BLOCK_TYPE;

			PayloadBlock();
			PayloadBlock(ibrcommon::BLOB::Reference ref);
			virtual ~PayloadBlock();

			ibrcommon::BLOB::Reference getBLOB() const;

			virtual Length getLength() const;
			virtual std::ostream &serialize(std::ostream &stream, Length &length) const;
			virtual std::istream &deserialize(std::istream &stream, const Length &length);

			/**
			 * serialize only a part of the payload
			 * @param stream The stream to serialize to.
			 * @param clip_offset The data offset of the payload.
			 * @param clip_length The length of the data.
			 * @return The previously given stream.
			 */
			std::ostream &serialize(std::ostream &stream, const Length &clip_offset, const Length &clip_length) const;

		private:
			ibrcommon::BLOB::Reference _blobref;
		};
	}
}

#endif /* PAYLOADBLOCK_H_ */
