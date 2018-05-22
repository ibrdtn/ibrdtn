/*
 * RSASHA256Stream.h
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

#ifndef RSASHA256STREAM_H_
#define RSASHA256STREAM_H_

#include "ibrcommon/Exceptions.h"
#include <openssl/evp.h>
#include <streambuf>
#include <iostream>
#include <vector>

namespace ibrcommon
{
	/**
	Calculates the signature of the data, which is streamed into it. The data
	will be hashed using SHA256 and this hash will be signed using RSA and the
	supplied key. You can stream as much data into it as you want. When calling
	getSign() the signature will be created and the stream reset.
	*/
	class RSASHA256Stream : public std::basic_streambuf<char, std::char_traits<char> >, public std::ostream
	{
	public:
		typedef std::char_traits<char> traits;

		/** The size of the buffer in which will be streamed. */
		static const size_t BUFF_SIZE = 2048;

		/**
		Creates a RSASHA256Stream object. And tells if it shall verify or
		calculate a signature.
		@param pkey the public or private key to be used
		@param verfiy if true, the signature will be verified, which will be given by
		calling getVerification()
		*/
		RSASHA256Stream(EVP_PKEY * const pkey, bool verfiy = false);

		/** cleans the output buffer and the openssl context */
		virtual ~RSASHA256Stream();

		/**
		Resets the stream and cleans up the context, which is used for calculating
		the signature.
		*/
		void reset();

		/**
		After signing this method will return a pair containing the return code
		and the signature itself.
		@return a pair containing return code and signature. the first item is the
		return code, which will be 1 for success and 0 for failure. The second
		item will be the signature.
		*/
		const std::pair<const int, const std::string> getSign();

		/**
		Verifies a foreign given signature.
		@param their_sign the signature to be verified
		@return returns 1 for a correct signature, 0 for failure and -1 if some other error occurred
		*/
		int getVerification(const std::string& their_sign);

	protected:
		/**
		Writes all unwritten data out of the buffer and into the openssl
		context for creation of the signature.
		@return maybe the last character
		*/
		virtual int sync();

		/**
		Writes all data from the buffer into the openssl context and clears the
		buffer.
		@param c the last char, for which was not room anymore in the buffer
		@return something that has to do with more characters following or not
		*/
		virtual traits::int_type overflow(traits::int_type = traits::eof());

	private:
		/** the buffer in which data will be streamed into */
		std::vector<char> out_buf_;

		/** the PKEY object containing the rsa key used for the signature */
		EVP_PKEY * const _pkey;

		/** tells wether a signature is created or verified */
		const bool _verify;

		/** the context in which the streamed data will be feed into for
		calculation of the hash/signature */
		EVP_MD_CTX * _ctx;

		/** tells if the context needs to be finalized to get a valid signature or
		verification */
		bool _sign_valid;
		/** if the return code of the verification is needed more times, it will
		be stored here for avoiding recalculating it*/
		int _return_code;
		/** the signature is stored here if it is needed more times and avoids
		recalculating it */
		std::string _sign;
	};
}
#endif /* RSASHA256STREAM_H_ */
