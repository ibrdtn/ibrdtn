/*
 * BlobBlock.h
 *
 * Copyright (C) 2017 IBR, TU Braunschweig
 *
 * Written-by: Tim Schubert <tim.schubert@tu-bs.de>
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

#ifndef IBRDTN_IBRDTN_IBRDTN_DATA_BLOBBLOCK_H_
#define IBRDTN_IBRDTN_IBRDTN_DATA_BLOBBLOCK_H_

#include "ibrdtn/data/Block.h"
#include "ibrcommon/data/BLOB.h"

namespace dtn
{
	namespace data
	{
		class BlobBlock: public Block
		{
		public:
			BlobBlock(dtn::data::block_t type)
					: Block(type)
			{
			}
			virtual ~BlobBlock()
			{
			}

			virtual ibrcommon::BLOB::Reference getBLOB() const = 0;
		};
	}
}

#endif /* IBRDTN_IBRDTN_IBRDTN_DATA_BLOBBLOCK_H_ */
