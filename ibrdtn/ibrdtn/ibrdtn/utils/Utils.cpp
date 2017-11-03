/*
 * Utils.cpp
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

#include "ibrdtn/utils/Utils.h"
#include "ibrcommon/data/BLOB.h"
#include "ibrdtn/data/Exceptions.h"
#include "ibrdtn/data/PayloadBlock.h"
#include "ibrdtn/utils/Clock.h"

#include <math.h>

namespace dtn
{
	namespace utils
	{
		void Utils::rtrim(std::string &str)
		{
			// trim trailing spaces
			std::string::size_type endpos = str.find_last_not_of(" \t");
			if( std::string::npos != endpos )
			{
				str = str.substr( 0, endpos+1 );
			}
		}

		void Utils::ltrim(std::string &str)
		{
			// trim leading spaces
			std::string::size_type startpos = str.find_first_not_of(" \t");
			if( std::string::npos != startpos )
			{
				str = str.substr( startpos );
			}
		}

		void Utils::trim(std::string &str)
		{
			ltrim(str);
			rtrim(str);
		}

		std::vector<std::string> Utils::tokenize(const std::string &token, const std::string &data, const std::string::size_type max)
		{
			std::vector<std::string> l;
			std::string value;

			// Skip delimiters at beginning.
			std::string::size_type pos = data.find_first_not_of(token, 0);

			std::string::size_type tokenPos = 0;

			while (pos != std::string::npos)
			{
				if (l.size() >= max)
				{
					// if maximum reached
					tokenPos = std::string::npos;
				}
				else
				{
					// Find first "non-delimiter".
					tokenPos = data.find_first_of(token, pos);
				}

				if (tokenPos == std::string::npos) {
					// No more tokens found, add last part to the vector.
					value = data.substr(pos);
					l.push_back(value);

					// exit the loop
					break;
				} else {
					// Found a token, add it to the vector.
					value = data.substr(pos, tokenPos - pos);
					l.push_back(value);
				}
				// Skip delimiters.  Note the "not_of"
				pos = data.find_first_not_of(token, tokenPos);
			}

			return l;
		}

		/**
		 * calculate the distance between two coordinates. (Latitude, Longitude)
		 */
		 double Utils::distance(double lat1, double lon1, double lat2, double lon2)
		 {
			const double r = 6371; //km

			double dLat = toRad( (lat2-lat1) );
			double dLon = toRad( (lon2-lon1) );

			double a = 	sin(dLat/2) * sin(dLat/2) +
						cos(toRad(lat1)) * cos(toRad(lat2)) *
						sin(dLon/2) * sin(dLon/2);
			double c = 2 * atan2(sqrt(a), sqrt(1-a));
			return r * c;
		 }

		 const double Utils::pi = 3.14159;

		 double Utils::toRad(double value)
		 {
			return value * pi / 180;
		 }

		void Utils::encapsule(dtn::data::Bundle &capsule, const std::list<dtn::data::Bundle> &bundles)
		{
			bool custody = false;
			dtn::data::Number exp_time = 0;

			try {
				const dtn::data::PayloadBlock &payload = capsule.find<dtn::data::PayloadBlock>();

				// get the stream object of the payload
				ibrcommon::BLOB::Reference ref = payload.getBLOB();

				// clear the hole payload
				ref.iostream().clear();

				// encapsule the bundles into the BLOB
				Utils::encapsule(ref, bundles);
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
				ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();

				// encapsule the bundles into the BLOB
				Utils::encapsule(ref, bundles);

				// add a new payload block
				capsule.push_back(ref);
			}

			// get maximum lifetime
			for (std::list<dtn::data::Bundle>::const_iterator iter = bundles.begin(); iter != bundles.end(); ++iter)
			{
				const dtn::data::Bundle &b = (*iter);

				// get the expiration time of this bundle
				dtn::data::Timestamp expt = dtn::utils::Clock::getExpireTime(b);

				// if this bundle expire later then use this lifetime
				if (expt > exp_time) exp_time = expt;

				// set custody to true if at least one bundle has custody requested
				if (b.get(dtn::data::PrimaryBlock::CUSTODY_REQUESTED)) custody = true;
			}

			// set the bundle flags
			capsule.set(dtn::data::PrimaryBlock::CUSTODY_REQUESTED, custody);

			// set the new lifetime
			capsule.lifetime = exp_time - capsule.timestamp;
		}

		void Utils::encapsule(ibrcommon::BLOB::Reference &ref, const std::list<dtn::data::Bundle> &bundles)
		{
			ibrcommon::BLOB::iostream stream = ref.iostream();

			// the number of encapsulated bundles
			dtn::data::Number elements(bundles.size());
			(*stream) << elements;

			// create a serializer
			dtn::data::DefaultSerializer serializer(*stream);

			// write bundle offsets
			std::list<dtn::data::Bundle>::const_iterator iter = bundles.begin();

			for (size_t i = 0; i < (bundles.size() - 1); i++, iter++)
			{
				const dtn::data::Bundle &b = (*iter);
				(*stream) << dtn::data::Number(serializer.getLength(b));
			}

			// serialize all bundles
			for (std::list<dtn::data::Bundle>::const_iterator iter = bundles.begin(); iter != bundles.end(); ++iter)
			{
				serializer << (*iter);
			}
		}

		void Utils::decapsule(const dtn::data::Bundle &capsule, std::list<dtn::data::Bundle> &bundles)
		{
			try {
				const dtn::data::PayloadBlock &payload = capsule.find<dtn::data::PayloadBlock>();
				ibrcommon::BLOB::iostream stream = payload.getBLOB().iostream();

				// read the number of bundles
				dtn::data::Number nob; (*stream) >> nob;

				// read all offsets
				for (size_t i = 0; (nob - 1) > i; ++i)
				{
					dtn::data::Number offset; (*stream) >> offset;
				}

				// create a deserializer for all bundles
				dtn::data::DefaultDeserializer deserializer(*stream);
				dtn::data::Bundle b;

				try {
					// read all bundles
					for (size_t i = 0; nob > i; ++i)
					{
						// deserialize the next bundle
						deserializer >> b;

						// add the bundle to the list
						bundles.push_back(b);
					}
				}
				catch (const dtn::InvalidDataException &ex) { };
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) { };
		}

		std::string Utils::toString(const dtn::data::Length &value)
		{
			std::stringstream ss;
			ss << value;
			return ss.str();
		}
	}
}
