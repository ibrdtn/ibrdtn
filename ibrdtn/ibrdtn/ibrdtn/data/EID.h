/*
 * EID.h
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

#ifndef EID_H_
#define EID_H_

#include <string>
#include <ibrcommon/Exceptions.h>
#include <ibrdtn/data/Number.h>
#include <map>

namespace dtn
{
	namespace data
	{
		class EID
		{
		public:
			enum Scheme {
				SCHEME_DTN = 0,
				SCHEME_CBHE = 1,
				SCHEME_EXTENDED = 2
			};

			/**
			 * Resolves a scheme in a string to the corresponding enum
			 */
			static Scheme resolveScheme(const std::string &s);

			/**
			 * Returns the name of a scheme
			 */
			static const std::string getSchemeName(const Scheme s);

			/**
			 * Map an application string to a CBHE number
			 */
			static Number getApplicationNumber(const std::string &app);

			EID();
			EID(const std::string &scheme, const std::string &ssp);
			EID(const std::string &value);

			/**
			 * Constructor for CBHE EIDs.
			 * @param node Node number.
			 * @param application Application number.
			 */
			EID(const dtn::data::Number &node, const dtn::data::Number &application);

			virtual ~EID();

			bool operator==(const EID &other) const;

			bool operator==(const std::string &other) const;

			bool operator!=(const EID &other) const;

			bool sameHost(const std::string &other) const;
			bool sameHost(const EID &other) const;

			bool operator<(const EID &other) const;
			bool operator>(const EID &other) const;

			std::string getString() const;

			void setApplication(const dtn::data::Number &app) noexcept;
			void setApplication(const std::string &app) noexcept;
			std::string getApplication() const noexcept;

			bool isApplication(const dtn::data::Number &app) const noexcept;
			bool isApplication(const std::string &app) const noexcept;

			std::string getHost() const noexcept;
			const std::string getScheme() const;
			const std::string getSSP() const;

			std::string getDelimiter() const;

			/**
			 * Return the EID with stripped application part
			 * @return The EID without any application specific part
			 */
			EID getNode() const noexcept;

			bool hasApplication() const;

			/**
			 * check if a EID is compressable.
			 * @return True, if the EID is compressable.
			 */
			bool isCompressable() const;

			/**
			 * Determine if this EID is null
			 * @return True, if the EID is null
			 */
			bool isNone() const;

			/**
			 * Get the compressed EID as two numeric values. Both values
			 * are set to zero if the EID is not compressable.
			 * @return A pair of two numeric values.
			 */
			typedef std::pair<Number, Number> Compressed;
			Compressed getCompressed() const;

		private:
			/**
			 * private constructor to create a modified EID
			 */
			EID(const Scheme scheme_type, const std::string &scheme, const std::string &ssp, const std::string &application);

			/**
			 * Extract the CBHE node and application from an SSP string
			 */
			static void extractCBHE(const std::string &ssp, Number &node, Number &app);

			/**
			 * Extract the DTN node and application from an SSP string
			 */
			static void extractDTN(const std::string &ssp, std::string &node, std::string &application);

			// abstract values
			Scheme _scheme_type;
			std::string _scheme;
			std::string _ssp;

			// DTN scheme
			// the ssp carries the node part
			std::string _application;

			// CBHE scheme
			Number _cbhe_node;
			Number _cbhe_application;

			// well-known CBHE numbers
			typedef std::map<std::string, Number> cbhe_map;
			static cbhe_map& getApplicationMap();
		};
	}
}

#endif /* EID_H_ */
