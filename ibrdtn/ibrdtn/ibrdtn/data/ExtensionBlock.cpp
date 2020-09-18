/*
 * ExtensionBlock.cpp
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

#include "ibrdtn/data/ExtensionBlock.h"
#include "ibrdtn/data/Exceptions.h"
#include "ibrdtn/data/ExtensionBlock.h"
#include <ibrcommon/thread/MutexLock.h>

namespace dtn
{
	namespace data
	{
		ExtensionBlock::FactoryList *ExtensionBlock::factories = NULL;

		ExtensionBlock::FactoryList::FactoryList()
		{ }

		ExtensionBlock::FactoryList::~FactoryList()
		{ }

		void ExtensionBlock::FactoryList::initialize()
		{
			static ibrcommon::Mutex mutex;
			ibrcommon::MutexLock l(mutex);

			if (ExtensionBlock::factories == NULL)
			{
				ExtensionBlock::factories = new ExtensionBlock::FactoryList();
			}
		}

		ExtensionBlock::Factory& ExtensionBlock::FactoryList::get(const block_t type) noexcept (false)
		{
			std::map<block_t, ExtensionBlock::Factory*>::iterator iter = fmap.find(type);

			if (iter != fmap.end())
			{
				return *(iter->second);
			}

			throw ibrcommon::Exception("Factory not available");
		}

		void ExtensionBlock::FactoryList::add(const block_t type, Factory *f) noexcept (false)
		{
			try {
				get(type);
				throw ibrcommon::Exception("extension block type already taken");
			} catch (const ibrcommon::Exception&) {
				fmap[type] = f;
			}
		}

		void ExtensionBlock::FactoryList::remove(const block_t type)
		{
			fmap.erase(type);
		}

		ExtensionBlock::Factory::Factory(block_t type)
		 : _type(type)
		{
			ExtensionBlock::FactoryList::initialize();
			ExtensionBlock::factories->add(type, this);
		}

		ExtensionBlock::Factory::~Factory()
		{
			ExtensionBlock::factories->remove(_type);
		}

		ExtensionBlock::Factory& ExtensionBlock::Factory::get(block_t type) noexcept (false)
		{
			ExtensionBlock::FactoryList::initialize();
			return ExtensionBlock::factories->get(type);
		}

		ExtensionBlock::ExtensionBlock()
		 : Block(0), _blobref(ibrcommon::BLOB::create())
		{
		}

		ExtensionBlock::ExtensionBlock(ibrcommon::BLOB::Reference ref)
		 : Block(0), _blobref(ref)
		{
		}

		ExtensionBlock::~ExtensionBlock()
		{
		}

		ibrcommon::BLOB::Reference ExtensionBlock::getBLOB() const
		{
			return _blobref;
		}

		void ExtensionBlock::setType(block_t type)
		{
			_blocktype = type;
		}

		Length ExtensionBlock::getLength() const
		{
			return _blobref.size();
		}

		std::ostream& ExtensionBlock::serialize(std::ostream &stream, Length &) const
		{
			ibrcommon::BLOB::Reference blobref = _blobref;
			ibrcommon::BLOB::iostream io = blobref.iostream();

			try {
				ibrcommon::BLOB::copy(stream, *io, io.size());
			} catch (const ibrcommon::IOException &ex) {
				throw dtn::SerializationFailedException(ex.what());
			}

			return stream;
		}

		std::istream& ExtensionBlock::deserialize(std::istream &stream, const Length &length)
		{
			// lock the BLOB
			ibrcommon::BLOB::iostream io = _blobref.iostream();

			// clear the blob
			io.clear();

			// check if the blob is ready
			if (!(*io).good()) throw dtn::SerializationFailedException("could not open BLOB for payload");

			try {
				ibrcommon::BLOB::copy(*io, stream, length);
			} catch (const ibrcommon::IOException &ex) {
				throw dtn::SerializationFailedException(ex.what());
			}

			// set block not processed bit
			set(dtn::data::Block::FORWARDED_WITHOUT_PROCESSED, true);

			return stream;
		}
	}
}
