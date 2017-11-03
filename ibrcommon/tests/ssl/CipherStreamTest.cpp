/*
 * CipherStreamTest.cpp
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

#include "ssl/CipherStreamTest.h"

#include <ibrcommon/refcnt_ptr.h>
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/data/File.h>
#include <ibrcommon/ssl/AES128Stream.h>
#include <ibrcommon/ssl/XORStream.h>
#include <ibrcommon/thread/MutexLock.h>
#include <stdlib.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <stdint.h>

CPPUNIT_TEST_SUITE_REGISTRATION (CipherStreamTest);

void CipherStreamTest::setUp()
{
	// generate some data
	std::ifstream rand("/dev/urandom", std::ios::in | std::ios::binary);
	rand.read(_plain_data, 1024);
}

void CipherStreamTest::tearDown()
{
}

std::string CipherStreamTest::getHex(char (&data)[1024])
{
	std::stringstream buf;

	for (int i = 0; i < 1024; ++i)
	{
		buf << std::hex << (int&)data[i] << ":";
	}

	buf << std::flush;

	return buf.str();
}

std::string CipherStreamTest::getHex(std::istream &stream)
{
	std::stringstream buf;
	unsigned int c = stream.get();

	while (!stream.eof())
	{
		buf << std::hex << c << ":";
		c = stream.get();
	}

	buf << std::flush;

	return buf.str();
}

bool CipherStreamTest::compare(char (&encrypted)[1024])
{
	for (int i = 0; i < 1024; ++i)
	{
		if (_plain_data[i] != encrypted[i])
		{
			return false;
		}
	}

	return true;
}

void CipherStreamTest::xorstream_test01()
{
	std::string key = "1234557890123456";
	std::string testdata((char*)&_plain_data, 1024);
	std::stringstream buffer;

	ibrcommon::XORStream decrypter(buffer, ibrcommon::CipherStream::CIPHER_DECRYPT, key);
	ibrcommon::XORStream crypter(decrypter, ibrcommon::CipherStream::CIPHER_ENCRYPT, key);

	// crypt some text
	crypter << testdata << std::flush;
	decrypter << std::flush;

	// check crypted data
	if (testdata != buffer.str())
	{
		throw ibrcommon::Exception("xorstream_test01 failed!");
	}
}

void CipherStreamTest::xorstream_test02()
{
	std::string key = "1234557890123456";
	std::string testdata((char*)&_plain_data, 1024);
	std::stringstream buffer;
	buffer << testdata;

	// reset the stream
	buffer.clear(); buffer.seekg(0); buffer.seekp(0);

	// create an encrypter
	ibrcommon::XORStream crypter(buffer, ibrcommon::CipherStream::CIPHER_ENCRYPT, key);

	// encrypt the content of the stream
	crypter << buffer.rdbuf() << std::flush;

	// reset the stream
	buffer.clear(); buffer.seekg(0); buffer.seekp(0);

	// check crypted data
	if (testdata == buffer.str())
	{
		throw ibrcommon::Exception("xorstream_test02 failed. text not encrypted!");
	}

	// reset the stream
	buffer.clear(); buffer.seekg(0); buffer.seekp(0);

	// create a decrypter
	ibrcommon::XORStream decrypter(buffer, ibrcommon::CipherStream::CIPHER_DECRYPT, key);

	// decrypt the stream
	decrypter << buffer.rdbuf() << std::flush;

	// reset the stream
	buffer.clear(); buffer.seekg(0); buffer.seekp(0);

	// check decrypted data
	if (testdata != buffer.str())
	{
		throw ibrcommon::Exception("xorstream_test02 failed. text not decrypted: " + buffer.str());
	}
}

void CipherStreamTest::xorstream_test03()
{
	std::string key = "1234557890123456";
	std::string testdata((char*)&_plain_data, 1024);

	ibrcommon::BLOB::Reference buffer = ibrcommon::BLOB::create();

	// write test data into the stream
	{
		ibrcommon::BLOB::iostream stream = buffer.iostream();
		(*stream) << testdata;
	}

	// create an encrypter
	{
		ibrcommon::BLOB::iostream stream = buffer.iostream();
		ibrcommon::XORStream crypter(*stream, ibrcommon::CipherStream::CIPHER_ENCRYPT, key);

		// encrypt the content of the stream
		crypter << (*stream).rdbuf() << std::flush;
	}

	// check crypted data
	{
		ibrcommon::BLOB::iostream stream = buffer.iostream();
		std::stringstream testbuffer; testbuffer << (*stream).rdbuf();
		if (testdata == testbuffer.str())
		{
			throw ibrcommon::Exception("xorstream_test03 failed. text not encrypted!");
		}
	}

	// create a decrypter
	{
		ibrcommon::BLOB::iostream stream = buffer.iostream();
		ibrcommon::XORStream decrypter(*stream, ibrcommon::CipherStream::CIPHER_DECRYPT, key);

		// decrypt the stream
		decrypter << (*stream).rdbuf() << std::flush;
	}

	// check decrypted data
	{
		ibrcommon::BLOB::iostream stream = buffer.iostream();

		std::stringstream testbuffer;
		testbuffer << (*stream).rdbuf();

		if (testdata != testbuffer.str())
		{
			throw ibrcommon::Exception("xorstream_test03 failed. text not decrypted: " + testbuffer.str());
		}
	}
}

void CipherStreamTest::xorstream_test04()
{
	std::string key = "1234557890123456";
	std::string testdata((char*)&_plain_data, 1024);

	ibrcommon::File f = ibrcommon::TemporaryFile(ibrcommon::File("/tmp"));

	// create test data
	{
		std::ofstream tmp(f.getPath().c_str(), std::ios::binary | std::ios::out);

		// write test data into the file
		tmp << testdata << std::flush;

		tmp.close();
	}

	// encrypt the data
	{
		std::fstream tmp(f.getPath().c_str(), std::ios::out | std::ios::in | std::ios::binary);

		// encrypt the data
		ibrcommon::XORStream crypter(tmp, ibrcommon::CipherStream::CIPHER_ENCRYPT, key);
		((ibrcommon::CipherStream&)crypter).encrypt(tmp);

		tmp.close();
	}

	// compare the result
	{
		std::ifstream tmp(f.getPath().c_str(), std::ios::binary | std::ios::in);

		std::stringstream testbuffer;
		testbuffer << tmp.rdbuf();

		if (testdata.length() != testbuffer.str().length())
		{
			throw ibrcommon::Exception("xorstream_test04 failed. encrypted text has wrong size");
		}

		if (testdata == testbuffer.str())
		{
			throw ibrcommon::Exception("xorstream_test04 failed. text not encrypted: " + getHex(testbuffer));
		}

		tmp.close();
	}

	f.remove();
}

void CipherStreamTest::aesstream_test01()
{
	uint32_t salt = 42;
	unsigned char iv[ibrcommon::AES128Stream::iv_len];
	unsigned char etag[ibrcommon::AES128Stream::tag_len];
	unsigned char key[ibrcommon::AES128Stream::key_size_in_bytes];

	for (unsigned int i = 0; i < ibrcommon::AES128Stream::key_size_in_bytes; ++i)
	{
		const std::string key_data = "1234557890123456";
		key[i] = key_data.c_str()[i % 10];
	}

	// test data string
	std::string testdata((char*)&_plain_data, 1024);

	// contains the encrypted / decrypted data
	std::stringstream data;

	// encrypt the test data
	{
		refcnt_ptr<ibrcommon::AES128Stream> crypt_stream(
				new ibrcommon::AES128Stream(ibrcommon::CipherStream::CIPHER_ENCRYPT, data, key, salt));

		// encrypt the data
		*crypt_stream << testdata << std::flush;

		// get the random IV
		crypt_stream->getIV(iv);

		// get the tag
		crypt_stream->getTag(etag);
	}

	// check the data
	if (data.str() == testdata)
	{
		throw ibrcommon::Exception("aesstream_test01 failed. data is not encrypted");
	}

	// decrypt the test data
	{

		refcnt_ptr<ibrcommon::AES128Stream> crypt_stream(
				new ibrcommon::AES128Stream(ibrcommon::CipherStream::CIPHER_DECRYPT, data, key, salt, iv));

		// decrypt the data
		((ibrcommon::CipherStream&)*crypt_stream).decrypt(data);

		// check the tag
		if (!crypt_stream->verify(etag))
		{
			throw ibrcommon::Exception("aesstream_test01 failed. tags not match!");
		}
	}

	// check the data
	if (data.str() != testdata)
	{
		throw ibrcommon::Exception("aesstream_test01 failed. data could not decrypted");
	}
}
