/*
 * Dictionary.h
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



#ifndef DICTIONARY_H_
#define DICTIONARY_H_

#include "ibrdtn/data/EID.h"
#include "ibrdtn/data/Number.h"
#include <list>
#include <sstream>
#include <list>
#include <stdint.h>

namespace dtn
{
	namespace data
	{
		class Bundle;

		class Dictionary
		{
		public:
			class EntryNotFoundException : public dtn::InvalidDataException
			{
			public:
				EntryNotFoundException(std::string what = "The requested dictionary entry is not available.") throw() : dtn::InvalidDataException(what)
				{
				};
			};

			/**
			 * create a empty dictionary
			 */
			Dictionary();

			/**
			 * create a dictionary with all EID of the given bundle
			 */
			Dictionary(const dtn::data::Bundle &bundle);

			/**
			 * copy constructor
			 */
			Dictionary(const Dictionary &d);

			/**
			 * assign operator
			 */
			Dictionary& operator=(const Dictionary &d);

			/**
			 * destructor
			 */
			virtual ~Dictionary();

			/**
			 * add an EID to the dictionary
			 */
			void add(const EID &eid);

			/**
			 * add a list of EIDs to the dictionary
			 */
			void add(const std::list<EID> &eids);

			/**
			 * add all EIDs of a bundle into the dictionary
			 */
			void add(const Bundle &bundle);

			/**
			 * return the eid for the reference [scheme,ssp]
			 */
			EID get(const Number &scheme, const Number &ssp);

			/**
			 * clear the dictionary
			 */
			void clear();

			/**
			 * returns the size of the bytearray
			 */
			Size getSize() const;

			/**
			 * returns the references of the given eid
			 */
			typedef std::pair<Number, Number> Reference;
			Reference getRef(const EID &eid) const;

			friend std::ostream &operator<<(std::ostream &stream, const dtn::data::Dictionary &obj);
			friend std::istream &operator>>(std::istream &stream, dtn::data::Dictionary &obj);

		private:
			bool exists(const std::string&) const;
			void add(const std::string&);
			Number get(const std::string&) const throw (EntryNotFoundException);

			std::stringstream _bytestream;
		};
	}
}

#endif /* DICTIONARY_H_ */
