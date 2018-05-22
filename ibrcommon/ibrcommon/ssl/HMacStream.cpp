/*
 * HMacStream.cpp
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

#include "ibrcommon/ssl/HMacStream.h"
#include "openssl_compat.h"

namespace ibrcommon
{
	HMacStream::HMacStream(const unsigned char * const key, const int key_size)
	 : HashStream(EVP_MAX_MD_SIZE, BUFF_SIZE), key_(key), key_size_(key_size)
	{
		ctx_ = HMAC_CTX_new();
		HMAC_Init_ex(ctx_, key_, key_size_, EVP_sha1(), NULL);
	}

	HMacStream::~HMacStream()
	{
		HMAC_CTX_free(ctx_);
	}

	void HMacStream::update(char *buf, const size_t size)
	{
		// hashing
		HMAC_Update(ctx_, (unsigned char*)buf, size);
	}

	void HMacStream::finalize(char * hash, unsigned int &size)
	{
		HMAC_Final(ctx_, (unsigned char*)hash, &size);
	}
}
