/*
 * Exceptions.h
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

#ifndef EXCEPTIONS_H_
#define EXCEPTIONS_H_

#include "ibrcommon/Exceptions.h"

#include <stdexcept>
#include <string>


namespace dtn
{
		/**
		 * If some data is invalid, this exception is thrown.
		 */
		class InvalidDataException : public ibrcommon::Exception
		{
			public:
				InvalidDataException(std::string what = "Invalid input data.") throw() : Exception(what)
				{
				};
		};

		class InvalidProtocolException : public dtn::InvalidDataException
		{
		public:
			InvalidProtocolException(std::string what = "The received data does not match the protocol.") throw() : dtn::InvalidDataException(what)
			{
			};
		};

		class SerializationFailedException : public dtn::InvalidDataException
		{
		public:
			SerializationFailedException(std::string what = "The serialization failed.") throw() : dtn::InvalidDataException(what)
			{
			};
		};

		class PayloadReceptionInterrupted : public dtn::SerializationFailedException
		{
		public:
			const size_t length;
			PayloadReceptionInterrupted(const size_t l, std::string what = "The payload reception has been interrupted.") throw() : dtn::SerializationFailedException(what), length(l)
			{
			};
		};

		class MissingObjectException : public ibrcommon::Exception
		{
		public:
			MissingObjectException(std::string what = "Object not available.") throw() : Exception(what)
			{
			};
		};

		class ConnectionInterruptedException : public ibrcommon::IOException
		{
		public:
			ConnectionInterruptedException() : ibrcommon::IOException("The connection has been interrupted.")
			{
			}
		};
}

#endif /*EXCEPTIONS_H_*/
