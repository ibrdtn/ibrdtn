/*
 * iostreamBIO.h
 *
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Stephen Roettger <roettger@ibr.cs.tu-bs.de>
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

#ifndef IOSTREAMBIO_H_
#define IOSTREAMBIO_H_

#include "ibrcommon/Exceptions.h"
#include <openssl/bio.h>

namespace ibrcommon
{

/*!
 * \brief This class is a wrapper for iostreams to the OpenSSL Input Output abstraction Layer (called BIO)
 */
class iostreamBIO
{
public:
	/*!
	 * \brief A Exception that is thrown on BIO related errors.
	 */
	class BIOException : public ibrcommon::Exception
	{
	public:
		BIOException(const std::string &what) throw() : Exception(what)
		{
		};
	};

	/*!
	 * \brief Creates a new iostreamBIO from a given iostream
	 * \param stream the iostream to use
	 * \warning The iostream is not freed on destruction of this object.
	 */
	explicit iostreamBIO(std::iostream *stream);

	/*!
	 * \brief get the internal BIO pointer
	 * \return the internal BIO (see OpenSSL documentation)
	 *
	 * \warning The BIO is freed on destruction of this object, dont use it afterwards (for example through an assigned SSL object or SSL_CTX)
	 */
	BIO *getBIO();

	/// the name that will be included in the BIO_METHOD->name field
	static const char * const name;
	/// type in the BIO_METHOD->type field
	static const int type = BIO_TYPE_SOURCE_SINK;

private:
	BIO *_bio;
	std::iostream *_stream;
};

}

#endif /* IOSTREAMBIO_H_ */
