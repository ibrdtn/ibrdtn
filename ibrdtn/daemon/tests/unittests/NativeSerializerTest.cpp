/*
 * NativeSerializerTest.cpp
 *
 *  Created on: 08.04.2013
 *      Author: morgenro
 */

#include "NativeSerializerTest.h"
#include "api/NativeSerializer.h"
#include "api/NativeSerializerCallback.h"
#include <ibrdtn/data/Bundle.h>
#include <iostream>

CPPUNIT_TEST_SUITE_REGISTRATION(NativeSerializerTest);
/*========================== tests below ==========================*/

/*=== BEGIN tests for NativeSerializer ===*/

void NativeSerializerTest::callbackStreamTest() {
	class Callback : public dtn::api::NativeSerializerCallback {
	public:
		size_t bundle_count;
		size_t block_count;
		size_t payload_count;

		Callback() : bundle_count(0), block_count(0), payload_count(0) {
		}

		virtual ~Callback() {
		}

		virtual void beginBundle(const dtn::data::PrimaryBlock &block) noexcept {
			bundle_count++;
		}

		virtual void endBundle() noexcept {
			bundle_count++;
		}

		virtual void beginBlock(const dtn::data::Block &block, const size_t payload_length) noexcept {
			block_count++;
		}

		virtual void endBlock() noexcept {
			block_count++;
		}

		virtual void payload(const char *data, const size_t len) noexcept {
			payload_count += len;
		}
	} _callback;

	dtn::api::NativeCallbackStream streambuf(_callback);

	std::ostream stream(&streambuf);
	stream << "Hello World" << std::flush;

	CPPUNIT_ASSERT_EQUAL((size_t)0, _callback.bundle_count);
	CPPUNIT_ASSERT_EQUAL((size_t)0, _callback.block_count);
	CPPUNIT_ASSERT_EQUAL((size_t)11, _callback.payload_count);
}

void NativeSerializerTest::serializeBundle() {
	class Callback : public dtn::api::NativeSerializerCallback {
	public:
		size_t bundle_count;
		size_t block_count;
		size_t payload_count;

		Callback() : bundle_count(0), block_count(0), payload_count(0) {
		}

		virtual ~Callback() {
		}

		virtual void beginBundle(const dtn::data::PrimaryBlock &block) noexcept {
			bundle_count++;
		}

		virtual void endBundle() noexcept {
			bundle_count++;
		}

		virtual void beginBlock(const dtn::data::Block &block, const size_t payload_length) noexcept {
			block_count++;
		}

		virtual void endBlock() noexcept {
			block_count++;
		}

		virtual void payload(const char *data, const size_t len) noexcept {
			payload_count += len;
		}
	} _callback;

	dtn::api::NativeSerializer serializer(_callback, dtn::api::NativeSerializer::BUNDLE_FULL);

	dtn::data::Bundle b;
	ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
	{
		ibrcommon::BLOB::iostream stream = ref.iostream();
		(*stream) << "Hello World";
	}

	b.push_back(ref);

	serializer << b;

	CPPUNIT_ASSERT_EQUAL((size_t)2, _callback.bundle_count);
	CPPUNIT_ASSERT_EQUAL((size_t)2, _callback.block_count);
	CPPUNIT_ASSERT_EQUAL((size_t)11, _callback.payload_count);
}


void NativeSerializerTest::setUp()
{
}

void NativeSerializerTest::tearDown()
{
}
