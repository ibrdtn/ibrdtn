/*
 * TestSerializer.cpp
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

#include "data/TestSerializer.h"
#include <ibrdtn/data/EID.h>
#include <cppunit/extensions/HelperMacros.h>

#include <ibrcommon/thread/MutexLock.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/Serializer.h>
#include <ibrdtn/data/BundleFragment.h>
#include <ibrdtn/data/AgeBlock.h>
#include <ibrdtn/data/ScopeControlHopLimitBlock.h>
#include <ibrdtn/data/BundleBuilder.h>
#include <iostream>
#include <sstream>

CPPUNIT_TEST_SUITE_REGISTRATION (TestSerializer);

void TestSerializer::hexdump(char c) {
  std::cout << std::hex << (int)c << " ";
}

void TestSerializer::setUp(void)
{
}

void TestSerializer::tearDown(void)
{
}

void TestSerializer::serializer_separate01(void)
{
	dtn::data::Bundle b;
	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

	std::stringstream ss1;
	dtn::data::PayloadBlock &p1 = b.push_back(ref);
	p1.addEID(dtn::data::EID("dtn://test1234/app1234"));

	std::stringstream ss2;
	dtn::data::PayloadBlock &p2 = b.push_back(ref);
	p2.addEID(dtn::data::EID("dtn://test1234/app1234"));

	dtn::data::SeparateSerializer(ss1) << p1;
	dtn::data::SeparateSerializer(ss2) << p2;

	ss1.clear(); ss1.seekp(0); ss1.seekg(0);
	ss2.clear(); ss2.seekp(0); ss2.seekg(0);

	dtn::data::Bundle b2;

	dtn::data::SeparateDeserializer(ss1, b2).readBlock();
	dtn::data::SeparateDeserializer(ss2, b2).readBlock();

	CPPUNIT_ASSERT_EQUAL( b2.size(), b.size() );
}

void TestSerializer::serializer_cbhe01(void)
{
	dtn::data::Bundle b;

	dtn::data::EID src("ipn:1.2");
	dtn::data::EID dst("ipn:2.3");
	dtn::data::EID report("ipn:6.1");

	dtn::data::EID::Compressed cbhe_eid = src.getCompressed();

	b.source = src;
	b.destination = dst;
	b.reportto = report;

	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	b.push_back(ref);

	std::stringstream ss;
	dtn::data::DefaultSerializer(ss) << b;

	dtn::data::Bundle b2;
	ss.clear(); ss.seekp(0); ss.seekg(0);
	dtn::data::DefaultDeserializer(ss) >> b2;

	CPPUNIT_ASSERT( b.source == b2.source );
	CPPUNIT_ASSERT( b.destination == b2.destination );
	CPPUNIT_ASSERT( b.reportto == b2.reportto );
	CPPUNIT_ASSERT( b.custodian == b2.custodian );
}

void TestSerializer::serializer_cbhe02(void)
{
	dtn::data::Bundle b;

	dtn::data::EID src("ipn:1.2");
	dtn::data::EID dst("ipn:2.3");

	b.source = src;
	b.destination = dst;

	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

	{
		ibrcommon::BLOB::iostream stream = ref.iostream();
		(*stream) << "0123456789";
	}

	b.push_back(ref);

	std::stringstream ss;
	dtn::data::DefaultSerializer(ss) << b;

	CPPUNIT_ASSERT_EQUAL((size_t)33, ss.str().length());
}

void TestSerializer::serializer_primaryblock_length(void)
{
	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://node1/app1");
	b.destination = dtn::data::EID("dtn://node2/app2");
	b.lifetime = 3600;
	b.timestamp = 12345678;
	b.sequencenumber = 1234;

	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	{
		ibrcommon::BLOB::iostream stream = ref.iostream();
		for (int i = 0; i < 10; ++i)
			(*stream) << "hello world" << std::flush;
	}

	dtn::data::Dictionary dict;

	// rebuild the dictionary
	dict.add(b.destination);
	dict.add(b.source);
	dict.add(b.reportto);
	dict.add(b.custodian);

	std::stringstream ss;
	dtn::data::DefaultSerializer ds(ss, dict);
	ds << (dtn::data::PrimaryBlock&)b;

	std::stringstream::pos_type written_len = ss.tellp();
	std::stringstream::pos_type calc_len = ds.getLength((dtn::data::PrimaryBlock&)b);

	CPPUNIT_ASSERT_EQUAL(written_len, calc_len);
}

void TestSerializer::serializer_block_length(void)
{
	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://node1/app1");
	b.destination = dtn::data::EID("dtn://node2/app2");
	b.lifetime = 3600;
	b.timestamp = 12345678;
	b.sequencenumber = 1234;

	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	{
		ibrcommon::BLOB::iostream stream = ref.iostream();
		for (int i = 0; i < 10; ++i)
			(*stream) << "hello world" << std::flush;
	}

	b.push_back(ref);
	b.push_front<dtn::data::ScopeControlHopLimitBlock>();

	for (dtn::data::Bundle::iterator iter = b.begin(); iter != b.end(); ++iter)
	{
		const dtn::data::Block &block = (**iter);

		std::stringstream ss;
		dtn::data::DefaultSerializer ds(ss);
		ds << block;

		size_t written_len = ss.tellp();
		size_t calc_len = ds.getLength(block);

		CPPUNIT_ASSERT_EQUAL(written_len, calc_len);
	}
}

void TestSerializer::serializer_bundle_length(void)
{
	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://node1/app1");
	b.destination = dtn::data::EID("dtn://node2/app2");
	b.lifetime = 3600;
	b.timestamp = 12345678;
	b.sequencenumber = 1234;

	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	{
		ibrcommon::BLOB::iostream stream = ref.iostream();
		for (int i = 0; i < 10; ++i)
			(*stream) << "hello world" << std::flush;
	}

	b.push_back(ref);
	b.push_front<dtn::data::ScopeControlHopLimitBlock>();

	std::stringstream ss;
	dtn::data::DefaultSerializer ds(ss);
	ds << b;

	size_t written_len = ss.tellp();
	size_t calc_len = ds.getLength(b);

	CPPUNIT_ASSERT_EQUAL(written_len, calc_len);
}

void TestSerializer::serializer_fragment_one(void)
{
	dtn::data::Bundle b;
	b.source = dtn::data::EID("dtn://node1/app1");
	b.destination = dtn::data::EID("dtn://node2/app2");
	b.lifetime = 3600;
	b.timestamp = 12345678;
	b.sequencenumber = 1234;

	std::stringstream ss;
	dtn::data::DefaultSerializer ds(ss);

	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

	// generate some payload data
	{
		ibrcommon::BLOB::iostream ios = ref.iostream();
		for (int i = 0; i < 100; ++i)
		{
			(*ios) << "0123456789";
		}
	}

	// add a payload block to the bundle
	b.push_back(ref);

	// serialize the bundle in fragments with only 100 bytes of payload
	for (int i = 0; i < 10; ++i)
	{
		ds << dtn::data::BundleFragment(b, i * 100, 100);
	}

	// jump to the first byte in the stringstream
	ss.seekg(0);

	for (int i = 0; i < 10; ++i)
	{
		dtn::data::Bundle fb;
		dtn::data::DefaultDeserializer(ss) >> fb;
		CPPUNIT_ASSERT(fb.get(dtn::data::PrimaryBlock::FRAGMENT));
		CPPUNIT_ASSERT_EQUAL(fb.find<dtn::data::PayloadBlock>().getLength(), (size_t)100);
	}
}

void TestSerializer::serializer_ipn_compression_length(void)
{
	dtn::data::Bundle ipnbundle;
	ipnbundle.source = dtn::data::EID("ipn:5.2");
	ipnbundle.destination = dtn::data::EID("ipn:1.2");

	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	{
		ibrcommon::BLOB::iostream stream = ref.iostream();
		for (int i = 0; i < 10; ++i)
			(*stream) << "hello world" << std::flush;
	}

	ipnbundle.push_back(ref);

	ipnbundle.lifetime = 3600;

	ipnbundle.push_front<dtn::data::ScopeControlHopLimitBlock>();

	std::stringstream ss;
	dtn::data::DefaultSerializer ds(ss);
	ds << ipnbundle;

	size_t written_len = ss.tellp();
	size_t calc_len = ds.getLength(ipnbundle);

	CPPUNIT_ASSERT_EQUAL(written_len, calc_len);
}

void TestSerializer::serializer_outin_binary(void)
{
	std::stringstream ss, ss1, ss2, payloadss;
	dtn::data::DefaultSerializer serializer(ss);
	dtn::data::DefaultDeserializer deserializer(ss);
	dtn::data::Bundle b1, b2;

	/* create Bundle */
	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	payloadss << "test payload" << std::endl;
	(*ref.iostream()) << payloadss.rdbuf();
	dtn::data::PayloadBlock &p1 = b1.push_back(ref);

	/* set all possible flags */
	p1.set( dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT, true );
	p1.set( dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED, true );
	p1.set( dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED, true );
	p1.set( dtn::data::Block::DISCARD_IF_NOT_PROCESSED, true );
	p1.set( dtn::data::Block::BLOCK_CONTAINS_EIDS, true );

	p1.addEID(dtn::data::EID("dtn://test1234/app1234"));

	b1.push_front<dtn::data::ScopeControlHopLimitBlock>();

	/* add unknown extension block */
	dtn::data::BundleBuilder builder(b1);
	dtn::data::Block &ext_block = builder.insert(42, 0);
	ext_block.set( dtn::data::Block::FORWARDED_WITHOUT_PROCESSED, true );

	dtn::data::ExtensionBlock &ext_block_cast = dynamic_cast<dtn::data::ExtensionBlock&>(ext_block);
	ibrcommon::BLOB::Reference ext_ref = ext_block_cast.getBLOB();
	{
		ibrcommon::BLOB::iostream ext_stream = ext_ref.iostream();
		(*ext_stream) << "Hello World" << std::flush;
	}

	/* serialize b1 and deserialize it into b2 */
	serializer << b1;
	deserializer >> b2;

	/* serialize b1 and b2 into stringstreams */
	dtn::data::DefaultSerializer(ss1) << b1;
	dtn::data::DefaultSerializer(ss2) << b2;

	/* compare strings */
	CPPUNIT_ASSERT_EQUAL( 0, ss1.str().compare(ss2.str()) );
}

void TestSerializer::serializer_outin_structure(void)
{
	std::stringstream ss, ss1, ss2, payloadss;
	dtn::data::DefaultSerializer serializer(ss);
	dtn::data::DefaultDeserializer deserializer(ss);
	dtn::data::Bundle b1, b2;

	/* create Bundle */
	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	payloadss << "test payload" << std::endl;
	(*ref.iostream()) << payloadss.rdbuf();
	dtn::data::PayloadBlock &p1 = b1.push_back(ref);

	/* set all possible flags */
	p1.set( dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT, true );
	p1.set( dtn::data::Block::TRANSMIT_STATUSREPORT_IF_NOT_PROCESSED, true );
	p1.set( dtn::data::Block::DELETE_BUNDLE_IF_NOT_PROCESSED, true );
	p1.set( dtn::data::Block::DISCARD_IF_NOT_PROCESSED, true );
	p1.set( dtn::data::Block::BLOCK_CONTAINS_EIDS, true );

	p1.addEID(dtn::data::EID("dtn://test1234/app1234"));

	b1.push_front<dtn::data::AgeBlock>();
	b1.push_front<dtn::data::ScopeControlHopLimitBlock>();

	/* add unknown extension block */
	dtn::data::BundleBuilder builder(b1);
	dtn::data::Block &ext_block = builder.insert(42, 0);
	ext_block.set( dtn::data::Block::FORWARDED_WITHOUT_PROCESSED, true );

	dtn::data::ExtensionBlock &ext_block_cast = dynamic_cast<dtn::data::ExtensionBlock&>(ext_block);
	ibrcommon::BLOB::Reference ext_ref = ext_block_cast.getBLOB();
	{
		ibrcommon::BLOB::iostream ext_stream = ext_ref.iostream();
		(*ext_stream) << "Hello World" << std::flush;
	}

	/* serialize b1 and deserialize it into b2 */
	serializer << b1;
	deserializer >> b2;

	/* serialize b1 and b2 into stringstreams */
	dtn::data::DefaultSerializer(ss1) << b1;
	dtn::data::DefaultSerializer(ss2) << b2;

	/* check structure */
	const dtn::data::PayloadBlock &p2 = b2.find<dtn::data::PayloadBlock>();
	CPPUNIT_ASSERT_EQUAL((size_t)1, p2.getEIDList().size());
	CPPUNIT_ASSERT(dtn::data::EID("dtn://test1234/app1234") == p2.getEIDList().front());

	CPPUNIT_ASSERT_NO_THROW( b2.find<dtn::data::AgeBlock>() );
	CPPUNIT_ASSERT_NO_THROW( b2.find<dtn::data::ScopeControlHopLimitBlock>() );
}
