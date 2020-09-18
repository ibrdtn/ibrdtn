/*
 * TLSExceptions.h
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

#ifndef TLSEXCEPTIONS_H_
#define TLSEXCEPTIONS_H_

#include "ibrcommon/Exceptions.h"

namespace ibrcommon
{
	class TLSException : public ibrcommon::Exception
	{
	public:
		TLSException(const std::string &what) noexcept : Exception(what)
		{
		}
	};

	class ContextCreationException : public TLSException
	{
	public:
		ContextCreationException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};

	class SSLCreationException : public TLSException
	{
	public:
		SSLCreationException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};

	class BIOCreationException : public TLSException
	{
	public:
		BIOCreationException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};

	class TLSHandshakeException : public TLSException
	{
	public:
		TLSHandshakeException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};

	class TLSNotInitializedException : public TLSException
	{
	public:
		TLSNotInitializedException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};

	class TLSCertificateFileException : public TLSException
	{
	public:
		TLSCertificateFileException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};

	class TLSKeyFileException : public TLSException
	{
	public:
		TLSKeyFileException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};

	class TLSCertificateVerificationException : public TLSException
	{
	public:
		TLSCertificateVerificationException(const std::string &what) noexcept : TLSException(what)
		{
		};
	};
}

#endif /* TLSEXCEPTIONS_H_ */
