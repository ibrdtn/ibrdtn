/*
 * PayloadBlock.cpp
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

#include "ibrdtn/data/PayloadBlock.h"
#include "ibrdtn/data/Exceptions.h"
#include <ibrcommon/thread/MutexLock.h>
#include <ibrcommon/Logger.h>

namespace dtn
{
	namespace data
	{
		const dtn::data::block_t PayloadBlock::BLOCK_TYPE = 1;

		PayloadBlock::PayloadBlock()
		 : BlobBlock(PayloadBlock::BLOCK_TYPE), _blobref(ibrcommon::BLOB::create())
		{
		}

		PayloadBlock::PayloadBlock(ibrcommon::BLOB::Reference ref)
		 : BlobBlock(PayloadBlock::BLOCK_TYPE), _blobref(ref)
		{
		}

		PayloadBlock::~PayloadBlock()
		{
		}

		ibrcommon::BLOB::Reference PayloadBlock::getBLOB() const
		{
			return _blobref;
		}

		Length PayloadBlock::getLength() const
		{
			return _blobref.size();
		}

		std::ostream& PayloadBlock::serialize(std::ostream &stream, Length &length) const
		{
			ibrcommon::BLOB::Reference blobref = _blobref;
			ibrcommon::BLOB::iostream io = blobref.iostream();

			try {
				ibrcommon::BLOB::copy(stream, *io, io.size());
				length += io.size();
			} catch (const ibrcommon::IOException &ex) {
				throw dtn::SerializationFailedException(ex.what());
			}

			return stream;
		}

		std::ostream& PayloadBlock::serialize(std::ostream &stream, const Length &clip_offset, const Length &clip_length) const
		{
			ibrcommon::BLOB::Reference blobref = _blobref;
			ibrcommon::BLOB::iostream io = blobref.iostream();

			try {
				(*io).seekg(clip_offset, std::ios::beg);
				ibrcommon::BLOB::copy(stream, *io, clip_length);
			} catch (const ibrcommon::IOException &ex) {
				throw dtn::SerializationFailedException(ex.what());
			}

			return stream;
		}

		std::istream& PayloadBlock::deserialize(std::istream &stream, const Length &length)
		{
			// lock the BLOB
			ibrcommon::BLOB::iostream io = _blobref.iostream();

			// clear the blob
			io.clear();

			// check if the blob is ready
			if (!(*io).good()) throw dtn::SerializationFailedException("could not open BLOB for payload");

			// set block not processed bit to false
			set(dtn::data::Block::FORWARDED_WITHOUT_PROCESSED, false);

			try {
				ibrcommon::BLOB::copy(*io, stream, length);
			} catch (const ibrcommon::IOException &ex) {
				throw dtn::PayloadReceptionInterrupted(length, ex.what());
			}

			return stream;
		}
	}
}
