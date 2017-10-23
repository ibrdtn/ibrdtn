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

#ifndef IBRCOMMON_EXCEPTIONS_H_
#define IBRCOMMON_EXCEPTIONS_H_

#include <stdexcept>
#include <string>


/**
 * @file Exceptions.h
 *
 * This file contains common exceptions used by class of the IBR common library.
 */

namespace ibrcommon
{
	/**
	 * Base class for all exceptions in the IBR common library.
	 */
	class Exception : public std::exception
	{
		public:
			Exception() throw()
			{};

			Exception(const exception&) throw()
			{};

			virtual ~Exception() throw()
			{};

			/**
			 * Get the explaining reason as string value.
			 * @return The reason as string value.
			 */
			virtual const char* what() const throw()
			{
				return _what.c_str();
			}

			/**
			 * constructor with attached string value as reason.
			 * @param what The detailed reason for this exception.
			 */
			Exception(std::string what) throw()
			{
				_what = what;
			};

		protected:
			std::string _what;
	};

	/**
	 * This is thrown if a method isn't implemented.
	 */
	class NotImplementedException : public Exception
	{
	public:
		NotImplementedException(std::string what = "This method isn't implemented.") throw() : Exception(what)
		{
		};
	};

	/**
	 * This is thrown if input/output error happens.
	 */
	class IOException : public Exception
	{
	public:
		IOException(std::string what = "Input/Output error.") throw() : Exception(what)
		{
		};
	};
}

#endif /*EXCEPTIONS_H_*/
