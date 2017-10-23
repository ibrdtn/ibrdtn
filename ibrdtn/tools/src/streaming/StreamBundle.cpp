/*
 * StreamBundle.cpp
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

#include "streaming/StreamBundle.h"
#include <ibrdtn/data/StreamBlock.h>

StreamBundle::StreamBundle()
 : _ref(ibrcommon::BLOB::create())
{
	dtn::data::StreamBlock &block = push_front<dtn::data::StreamBlock>();
	block.setSequenceNumber(0);

	push_back(_ref);
}

StreamBundle::StreamBundle(const dtn::data::Bundle &b)
 : dtn::data::Bundle(b), _ref(find<dtn::data::PayloadBlock>().getBLOB())
{
}

StreamBundle::~StreamBundle()
{
}

void StreamBundle::append(const char* data, size_t length)
{
	ibrcommon::BLOB::iostream stream = _ref.iostream();
	(*stream).seekp(0, std::ios::end);
	(*stream).write(data, length);
}

void StreamBundle::clear()
{
	ibrcommon::BLOB::iostream stream = _ref.iostream();
	stream.clear();

	// increment the sequence number
	try {
		dtn::data::StreamBlock &block = find<dtn::data::StreamBlock>();
		block.setSequenceNumber(block.getSequenceNumber() + 1);
	} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };
}

size_t StreamBundle::size()
{
	return _ref.size();
}
