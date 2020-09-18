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


using namespace std;

namespace dtn
{
		/**
		 * If some data is invalid, this exception is thrown.
		 */
		class InvalidDataException : public ibrcommon::Exception
		{
			public:
				InvalidDataException(string what = "Invalid input data.") noexcept : Exception(what)
				{
				};
		};

		class InvalidProtocolException : public dtn::InvalidDataException
		{
		public:
			InvalidProtocolException(string what = "The received data does not match the protocol.") noexcept : dtn::InvalidDataException(what)
			{
			};
		};

		class SerializationFailedException : public dtn::InvalidDataException
		{
		public:
			SerializationFailedException(string what = "The serialization failed.") noexcept : dtn::InvalidDataException(what)
			{
			};
		};

		class PayloadReceptionInterrupted : public dtn::SerializationFailedException
		{
		public:
			const size_t length;
			PayloadReceptionInterrupted(const size_t l, string what = "The payload reception has been interrupted.") noexcept : dtn::SerializationFailedException(what), length(l)
			{
			};
		};

		class MissingObjectException : public ibrcommon::Exception
		{
		public:
			MissingObjectException(string what = "Object not available.") noexcept : Exception(what)
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
