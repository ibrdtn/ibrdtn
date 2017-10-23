/*
 * Block.cpp
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

#include "ibrdtn/data/Block.h"
#include "ibrdtn/data/Bundle.h"
#include <iostream>


namespace dtn
{
	namespace data
	{
		Block::Block(block_t blocktype)
		 : _blocktype(blocktype)
		{
		}

		Block::~Block()
		{
		}

		Block& Block::operator=(const Block &block)
		{
			bool last_block = _procflags.getBit(LAST_BLOCK);
			_procflags = block._procflags;
			_procflags.setBit(LAST_BLOCK, last_block);

			_eids = block._eids;
			return (*this);
		}

		bool Block::operator==(const block_t &id) const
		{
			return (id == _blocktype);
		}

		void Block::addEID(const EID &eid)
		{
			_eids.push_back(eid);

			// add proc. flag
			_procflags.setBit(Block::BLOCK_CONTAINS_EIDS, true);
		}

		void Block::clearEIDs()
		{
			_eids.clear();

			// clear proc. flag
			_procflags.setBit(Block::BLOCK_CONTAINS_EIDS, false);
		}

		const Block::eid_list& Block::getEIDList() const
		{
			return _eids;
		}

		void Block::set(ProcFlags flag, const bool &value)
		{
			_procflags.setBit(flag, value);
		}

		bool Block::get(ProcFlags flag) const
		{
			return _procflags.getBit(flag);
		}

		const Bitset<Block::ProcFlags>& Block::getProcessingFlags() const
		{
			return _procflags;
		}

		Length Block::getLength_strict() const
		{
			return getLength();
		}

		std::ostream& Block::serialize_strict(std::ostream &stream, Length &length) const
		{
			return serialize(stream, length);
		}
	}
}
