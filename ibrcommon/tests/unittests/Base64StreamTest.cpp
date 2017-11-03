/*
 * Base64StreamTest.cpp
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

#include "Base64StreamTest.h"
#include <ibrcommon/data/Base64Stream.h>
#include <ibrcommon/data/Base64Reader.h>
#include <cstdlib>
#include <sstream>
#include <string>

CPPUNIT_TEST_SUITE_REGISTRATION(Base64StreamTest);

/*========================== tests below ==========================*/

/*=== BEGIN tests for class 'Base64StreamTest' ===*/
void Base64StreamTest::testEncode()
{
	std::stringstream ss;
	ibrcommon::Base64Stream b64(ss);
	b64 << "Hallo Welt" << std::endl << std::flush;

	CPPUNIT_ASSERT(ss.str().length() > 0);
}

void Base64StreamTest::testDecode()
{
	std::stringstream ss_plain;
	std::stringstream ss_encoded;
	std::stringstream ss_decoded;

	// test data
	for (int i = 0; i < 100; ++i)
		ss_plain << "0123456789";

	// encode the data
	ibrcommon::Base64Stream(ss_encoded, false, 80) << ss_plain.rdbuf() << std::flush;

	// decode the data
	ibrcommon::Base64Stream(ss_decoded, true) << ss_encoded.rdbuf() << std::flush;

	CPPUNIT_ASSERT_EQUAL(ss_plain.str().length(), ss_decoded.str().length());
	CPPUNIT_ASSERT_EQUAL(ss_plain.str(), ss_decoded.str());
}

void Base64StreamTest::testFileReference()
{
	std::stringstream ss_encoded;
	std::stringstream ss_decoded;

	// open reference file
	std::string filename("../base64-enc.dat");
	std::ifstream f(filename);
	CPPUNIT_ASSERT_MESSAGE("Error reading file: " + filename, f.good());

	// decode the data
	ss_decoded << ibrcommon::Base64Reader(f, 46207).rdbuf() << std::flush;
	CPPUNIT_ASSERT_MESSAGE("Error in data decode", f.good());

	std::ifstream fd("../base64-dec.dat");
	compare(fd, ss_decoded);

	// read the last two '\n'
	CPPUNIT_ASSERT_EQUAL( (char)f.get(), (char)'\n' );
	CPPUNIT_ASSERT_EQUAL( (char)f.get(), (char)'\n' );
	CPPUNIT_ASSERT(f.good());
}

void Base64StreamTest::testReader()
{
	for (int j = 43000; j < 43010; ++j)
	{
		std::cout << "+" << std::flush;
//	int j = 42690;

//		std::fstream ss_plain("plain.dat", std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out);
//		std::fstream ss_encoded("encoded.dat", std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out);
//		std::fstream ss_decoded("decoded.dat", std::ios::binary | std::ios::trunc | std::ios::in | std::ios::out);
		std::stringstream ss_plain;
		std::stringstream ss_encoded;
		std::stringstream ss_decoded;

		// encode the data
		ibrcommon::Base64Stream is(ss_encoded, false, 75);

		// test data
		for (int i = 0; i < j; ++i)
		{
			char c = (i % 256);
			ss_plain.put(c);
			is.put(c);
		}

		is.flush(); ss_plain.flush();

		ss_encoded.clear();
		ss_encoded.seekp(0);
		ss_encoded.seekg(0);

		// decode the data
		ss_decoded << ibrcommon::Base64Reader(ss_encoded, j).rdbuf() << std::flush;

//		if (ss_plain.str().length() != ss_decoded.str().length())
//		{
//			ss_encoded.clear(); ss_encoded.seekg(0);
//
//			std::cout << std::endl;
//			std::cout << ss_encoded.rdbuf() << std::endl;
//		}

		CPPUNIT_ASSERT_EQUAL(ss_plain.str().length(), ss_decoded.str().length());
		compare(ss_plain, ss_decoded);
	}
}

void Base64StreamTest::compare(std::istream &s1, std::istream &s2)
{
	s1.clear(); s1.seekg(0);
	s2.clear(); s2.seekg(0);

	while (s1.good())
	{
		CPPUNIT_ASSERT(s2.good());
		CPPUNIT_ASSERT_EQUAL(s1.get(), s2.get());
	}
}

/*=== END   tests for class 'Base64StreamTest' ===*/

void Base64StreamTest::setUp()
{
}

void Base64StreamTest::tearDown()
{
}
