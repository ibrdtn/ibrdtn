/*
 * SecurityKey.cpp
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

#include "ibrdtn/security/SecurityKey.h"
#include <ibrcommon/ssl/SHA256Stream.h>
#include <ibrcommon/Logger.h>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <openssl/sha.h>
#include <openssl/pem.h>
#include <openssl/err.h>

namespace dtn
{
	namespace security
	{
		SecurityKey::SecurityKey()
		 : type(KEY_UNSPEC), trustlevel(NONE)
		{}

		SecurityKey::~SecurityKey()
		{}

		void SecurityKey::free(RSA* key)
		{
			RSA_free(key);
		}

		void SecurityKey::free(EVP_PKEY* key)
		{
			EVP_PKEY_free(key);
		}

		bool SecurityKey::operator==(const SecurityKey &key)
		{
			return getFingerprint() == key.getFingerprint();
		}

		ibrcommon::File SecurityKey::getMetaFilename() const
		{
			return ibrcommon::File(file.getPath() + ".txt");
		}

		const std::string SecurityKey::getData() const
		{
			std::ifstream stream(file.getPath().c_str(), std::ios::in);
			std::stringstream ss;

			ss << stream.rdbuf();

			stream.close();

			return ss.str();
		}

		RSA* SecurityKey::getRSA() const
		{
			switch (type)
			{
			case KEY_PRIVATE:
				return getPrivateRSA();
			case KEY_PUBLIC:
				return getPublicRSA();
			default:
				return NULL;
			}
		}

		EVP_PKEY* SecurityKey::getEVP() const
		{
			EVP_PKEY* ret = EVP_PKEY_new();
			FILE * pkey_file = fopen(file.getPath().c_str(), "r");

			switch (type)
			{
				case KEY_PRIVATE:
				{
					ret = PEM_read_PrivateKey(pkey_file, &ret, NULL, NULL);
					break;
				}

				case KEY_PUBLIC:
				{
					ret = PEM_read_PUBKEY(pkey_file, &ret, NULL, NULL);
					break;
				}

				default:
					ret = NULL;
					break;
			}

			fclose(pkey_file);
			return ret;
		}

		const std::string SecurityKey::getFingerprint() const
		{
			switch (type)
			{
				case KEY_PRIVATE:
				{
					RSA* rsa = getPrivateRSA();
					std::string ret = getFingerprint(rsa);
					free(rsa);
					return ret;
				}
				case KEY_PUBLIC:
				{
					RSA* rsa = getPublicRSA();
					std::string ret = getFingerprint(rsa);
					free(rsa);
					return ret;
				}
				default:
				{
					return getFingerprint(file);
				}
			}
		}

		RSA* SecurityKey::getPrivateRSA() const
		{
			RSA *rsa = RSA_new();

			FILE * rsa_pkey_file = fopen(file.getPath().c_str(), "r");
			if (!rsa_pkey_file) {
				IBRCOMMON_LOGGER_ex(critical) << "Failed to open " << file.getPath() << IBRCOMMON_LOGGER_ENDL;
				throw ibrcommon::Exception("Failed to open " + file.getPath());
			}
			if (!PEM_read_RSAPrivateKey(rsa_pkey_file, &rsa, NULL, NULL)) {
				IBRCOMMON_LOGGER_ex(critical) << "Error loading RSA private key file: " << file.getPath() << IBRCOMMON_LOGGER_ENDL;
				ERR_print_errors_fp(stderr);
				throw ibrcommon::Exception("Error loading RSA private key file: " + file.getPath());
			}
			fclose(rsa_pkey_file);
			return rsa;
		}

		RSA* SecurityKey::getPublicRSA() const
		{
			RSA *rsa = RSA_new();

			FILE * rsa_pkey_file = fopen(file.getPath().c_str(), "r");
			if (!rsa_pkey_file) {
				IBRCOMMON_LOGGER_ex(critical) << "Failed to open " << file.getPath() << IBRCOMMON_LOGGER_ENDL;
				throw ibrcommon::Exception("Failed to open " + file.getPath());
			}
			if (!PEM_read_RSA_PUBKEY(rsa_pkey_file, &rsa, NULL, NULL)) {
				IBRCOMMON_LOGGER_ex(critical) << "Error loading RSA public key file: " << file.getPath() << IBRCOMMON_LOGGER_ENDL;
				ERR_print_errors_fp(stderr);
				throw ibrcommon::Exception("Error loading RSA public key file: " + file.getPath());
			}
			fclose(rsa_pkey_file);
			return rsa;
		}

		std::string SecurityKey::getFingerprint(const ibrcommon::File &file)
		{
			// create a hash stream
			ibrcommon::SHA256Stream sha;

			// open the file
			std::ifstream stream(file.getPath().c_str());

			// hash the contents of the file
			if (!stream.good()) {
				sha << stream.rdbuf() << std::flush;
			}

			// convert bytes to hex fingerprint
			std::stringstream fingerprint;
			while (sha.good())
			{
				fingerprint << std::hex << std::setw(2) << std::setfill('0') << (int)sha.get();
			}

			return fingerprint.str();
		}

		std::string SecurityKey::getFingerprint(RSA* rsa)
		{
			unsigned char *p = NULL;
			int length = i2d_RSA_PUBKEY(rsa, &p);

			std::string ret = "";
			if (length > 0)
			{
				ret = std::string((const char*)p, length);
			}
			else
			{
				OPENSSL_free(p);
				free(rsa);
				throw(ibrcommon::Exception("Error while parsing rsa key"));
			}

			unsigned char hash[SHA256_DIGEST_LENGTH];
			SHA256(p, length, hash);

			std::stringstream stream;
			for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
			{
				stream << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
			}

			OPENSSL_free(p);
			return stream.str();
		}
	}
}
