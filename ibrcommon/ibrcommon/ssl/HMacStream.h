/*
 * HMacStream.h
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

#ifndef HMACSTREAM_H_
#define HMACSTREAM_H_

#include "ibrcommon/ssl/HashStream.h"
#include <openssl/hmac.h>

namespace ibrcommon
{
	class HMacStream : public ibrcommon::HashStream
	{
	public:
		// The size of the input and output buffers.
		static const size_t BUFF_SIZE = 2048;

		HMacStream(const unsigned char * const key, const int key_size);
		virtual ~HMacStream();

	protected:
		virtual void update(char *buf, const size_t size);
		virtual void finalize(char * hash, unsigned int &size);

	private:
		const unsigned char * const key_;
		const int key_size_;

		HMAC_CTX* ctx_;
	};
}

#endif /* HMACSTREAM_H_ */
