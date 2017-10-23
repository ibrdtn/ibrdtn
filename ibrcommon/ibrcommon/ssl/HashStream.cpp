/*
 * HashStream.cpp
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

#include "HashStream.h"
#include <sstream>

namespace ibrcommon
{
	HashStream::HashStream(const unsigned int hash, const size_t buffer)
	 : std::iostream(this), data_buf_(buffer), data_size_(buffer), hash_buf_(hash), hash_size_(hash), final_(false)
	{
		// Initialize get pointer.  This should be zero so that underflow is called upon first read.
		setg(0, 0, 0);
		setp(&data_buf_[0], &data_buf_[0] + data_size_ - 1);
	}

	HashStream::~HashStream()
	{
	}

	std::string HashStream::extract(std::istream &stream)
	{
		std::stringstream buf;
		buf << stream.rdbuf();
		return buf.str();
	}

	int HashStream::sync()
	{
		int ret = std::char_traits<char>::eq_int_type(this->overflow(
				std::char_traits<char>::eof()), std::char_traits<char>::eof()) ? -1
				: 0;

		return ret;
	}

	std::char_traits<char>::int_type HashStream::overflow(std::char_traits<char>::int_type c)
	{
		char *ibegin = &data_buf_[0];
		char *iend = pptr();

		// mark the buffer as free
		setp(&data_buf_[0], &data_buf_[0] + data_size_ - 1);

		if (!std::char_traits<char>::eq_int_type(c, std::char_traits<char>::eof()))
		{
			*iend++ = std::char_traits<char>::to_char_type(c);
		}

		// if there is nothing to send, just return
		if ((iend - ibegin) == 0)
		{
			return std::char_traits<char>::not_eof(c);
		}

		// hashing
		update(&data_buf_[0], (iend - ibegin));

		return std::char_traits<char>::not_eof(c);
	}

	std::char_traits<char>::int_type HashStream::underflow()
	{
		// TODO: add seek mechanisms to reset the istream

		if (!final_)
		{
			finalize(&hash_buf_[0], hash_size_);
			final_ = true;

			// Since the input buffer content is now valid (or is new)
			// the get pointer should be initialized (or reset).
			setg(&hash_buf_[0], &hash_buf_[0], &hash_buf_[0] + hash_size_);

			return std::char_traits<char>::not_eof(hash_buf_[0]);
		}

		return std::char_traits<char>::eof();
	}
}
