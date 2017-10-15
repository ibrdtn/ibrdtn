/**
 * Discovery.h
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Roland Hieber <r.hieber@tu-bs.de>
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
#ifndef DISCOVERY_H_
#define DISCOVERY_H_

#include <ibrcommon/Exceptions.h>
#include <ibrdtn/data/BundleString.h>
#include <ibrdtn/data/Number.h>
#include <stdint.h>
#include <string>
#include <sstream>

namespace dtn {
	namespace net {
		class WrongVersionException : public ibrcommon::Exception
		{
			public:
				WrongVersionException(std::string what = "(n/a)") throw()
				 : ibrcommon::Exception(std::string("Unspecified or unsupported "
							"discovery protocol version: ").append(what)) {}
				virtual ~WrongVersionException() throw() {}
		};

		class IllegalServiceException : public ibrcommon::Exception
		{
			public:
				IllegalServiceException(std::string what = "(n/a)") throw()
				 : ibrcommon::Exception(std::string("No such service, or illegal "
							"parameters for service: ").append(what)) {}
				virtual ~IllegalServiceException() throw() {}
		};

		class ParseException : public ibrcommon::Exception
		{
			size_t bytes_read_;
			public:
				ParseException(std::string what = "(n/a)", size_t bytes_read = 0) throw()
				 : ibrcommon::Exception(), bytes_read_(bytes_read)
				{
					std::ostringstream ss;
					ss << "Parse error at input byte " << bytes_read << ": " << what;
					_what = ss.str();
				}
				virtual ~ParseException() throw() {}
				inline const size_t getBytesRead() const { return bytes_read_; }
		};

		/** Definitions common to the Discovery* classes */
		namespace Discovery {
			enum Protocol
			{
				DTND_IPDISCOVERY = 0x00, /*< Legacy DTND discovery */
				DISCO_VERSION_00 = 0x01, /*< IPND draft 00 */
				DISCO_VERSION_01 = 0x02, /*< IPND draft 01 */
				DISCO_VERSION_02 = 0x04, /*< IPND draft 02 */
				DISCO_UNSPEC     = 0xFF, /*< Unspecified discovery protocol */
			};
		}

		/** Definitions related to IPND */
		namespace ipnd {

			/**
			 * @addtogroup ipnd02tlv IPND Draft 02 TLV types
			 * IPND TLV definitions, according to Draft 02, Sections 2.6.2 and 2.6.3
			 * @see http://tools.ietf.org/html/draft-irtf-dtnrg-ipnd-02#section-2.6.2
			 *
			 * Concept: Given a struct @c TYPE:
			 * @li @c TYPE::Tag is the IPND TLV tag of that datatype
			 * @li @c TYPE::Length is the length of the value in bytes. If @c 0, the
			 *   value is encoded as SDNV, and no length is prefixed .
			 *   If @c -1, the value is explicitly prefixed with a length, encoded
			 *   as SDNV.
			 *   If greater than @c 0, the length is implicitly encoded in the @c Tag.
			 * @li @c TYPE::Type, if specified, is the mapped C++ type for
			 *   non-constructed data types.
			 * @{
			 */
			struct BOOLEAN { enum { Tag = 0, Length =  1 }; typedef bool     Type; };
			struct UINT64  { enum { Tag = 1, Length =  0 }; typedef dtn::data::Number Type; };
			struct SINT64  { enum { Tag = 2, Length =  0 }; typedef dtn::data::Number Type; };
			struct FIXED16 { enum { Tag = 3, Length =  2 }; typedef uint16_t Type; };
			struct FIXED32 { enum { Tag = 4, Length =  4 }; typedef uint32_t Type; };
			struct FIXED64 { enum { Tag = 5, Length =  8 }; typedef uint64_t Type; };
			struct FLOAT   { enum { Tag = 6, Length =  2 }; typedef float    Type; };
			struct DOUBLE  { enum { Tag = 7, Length =  4 }; typedef double   Type; };
			struct STRING  { enum { Tag = 8, Length = -1 }; typedef dtn::data::BundleString Type; };
			struct BYTES   { enum { Tag = 9, Length = -1 }; typedef dtn::data::BundleString Type; };

			/** Constructed type <tt>CLA_TCP_v4 = { address (fixed32), port (fixed16) }</tt> */
			struct CLA_TCP_v4 { enum { Tag =  64, Length = -1 }; };
			/** Constructed type <tt>CLA_UDP_v4 = { address (fixed32), port (fixed16) }</tt> */
			struct CLA_UDP_v4 { enum { Tag =  65, Length = -1 }; };
			/** Constructed type <tt>CLA_TCP_v6 = { address (bytes), port (fixed16) }</tt> */
			struct CLA_TCP_v6 { enum { Tag =  66, Length = -1 }; };
			/** Constructed type <tt>CLA_TCP_v6 = { address (bytes), port (fixed16) }</tt> */
			struct CLA_UDP_v6 { enum { Tag =  67, Length = -1 }; };

			/** Private constructed type <tt>CLA_DGRAM_UDP = { address (string) }</tt> */
			struct CLA_DGRAM_UDP      { enum { Tag = 187, Length = -1 }; };
			/** Private constructed type <tt>CLA_DGRAM_ETHERNET = { address (string) }</tt> */
			struct CLA_DGRAM_ETHERNET { enum { Tag = 188, Length = -1 }; };
			/** Private constructed type <tt>CLA_DGRAM_LOWPAN = { address (string) }</tt> */
			struct CLA_DGRAM_LOWPAN   { enum { Tag = 189, Length = -1 }; };
			/** Private constructed type <tt>CLA_LOWPAN = { address (fixed16), port (fixed16) }</tt> */
			struct CLA_LOWPAN { enum { Tag = 190, Length = -1 }; };
			/** Private constructed type <tt>CLA_EMAIL = { address (string) }</tt> */
			struct CLA_EMAIL  { enum { Tag = 191, Length = -1 }; };
			/** Private constructed type <tt>CLA_DHT = { port (fixed16), proxy (boolean) }</tt> */
			struct CLA_DHT    { enum { Tag = 192, Length = -1 }; };
			/** Private constructed type <tt>CLA_DTNTP = { version (uint64), quality (string),
			 *                                             timestamp (uint64) }</tt> */
			struct CLA_DTNTP  { enum { Tag = 193, Length = -1 }; };
			/** @} */

			/**
			 * @tparam T a @ref ipnd02tlv "IPND TLV type"
			 * @returns the string representation of @c T
			 */
			template<typename T> const std::string tagString() { return ""; }

			// Workaround, function templates cannot be specialized partially
			template<class T, int Length> struct Helpers
			{
				static const size_t read(std::istream& stream, typename T::Type& buf)
				{
					stream.read((char *) &buf, Length);
					return stream.gcount();
				}
				static const size_t write(std::ostream& stream, const typename T::Type& buf)
				{
					stream.write((char *) &buf, Length);
					return Length;
				}
			};
			/** 
			 * Specialization for T::Length = 0 => T::Type is SDNV
			 * @tparam T T::Type must have an operator<<(std::istream, T::Type) and
			 *   a T::Type::getLength() which returns size_t.
			 */
			template<class T> struct Helpers<T, 0>
			{
				static const size_t read(std::istream& stream, typename T::Type& buf)
				{
					stream >> buf;
					return buf.getLength();
				}
				static const size_t write(std::ostream& stream, const typename T::Type& buf)
				{
					stream << buf;
					return buf.getLength();
				}
			};
			/** 
			 * Specialization for T::Length = -1 => T::Type is SDNV-prefixed
			 * @tparam T T::Type must have an operator<<(std::istream, T::Type) and
			 *   a T::Type::getLength() which returns size_t.
			 */
			template<class T> struct Helpers<T, -1> : Helpers<T, 0> {};

			/**
			 * Read a non-constructed TLV entity. Checks tag and length.
			 * @tparam T a non-constructed @ref ipnd02tlv "IPND TLV type"
			 * @param stream Input stream to read from
			 * @param buf Buffer to write the value to
			 * @returns number of bytes read from @c stream
			 * @throws ParseException if an error occured while reading, or if the
			 *  tag or length as read from the stream do not match the requested type.
			 */
			template<class T>
			size_t read(std::istream& stream, typename T::Type& buf) throw (ParseException)
			{
				uint8_t tag;
				size_t length = T::Length;
				size_t bytes_read = 0, bytes_temp = 0;
				std::ostringstream err;

				// tag
				stream.read((char *)(&tag), 1);
				if (!stream.good())
				{
					err << "could not parse IPND " << tagString<T>() << " tag";
					throw ParseException(err.str(), bytes_read);
				}
				if (tag != T::Tag)
				{
					err << "expected IPND " << tagString<T>() << " tag (" << std::dec
						<< (int) T::Tag << "), but found " << std::dec << (int) tag;
					throw ParseException(err.str(), bytes_read);
				}
				bytes_read += stream.gcount();

				// length(?) and value
				bytes_temp = Helpers<T, T::Length>::read(stream, buf);
				if (!stream.good())
				{
					err << "could not parse IPND " << tagString<T>() << " value";
					throw ParseException(err.str(), bytes_read);
				}
				bytes_read += bytes_temp;

				return bytes_read;
			}

			/**
			 * Write a non-constructed TLV entity, with tag and length.
			 * @tparam T a non-constructed @ref ipnd02tlv "IPND TLV type"
			 * @param stream Input stream to read from
			 * @param buf Buffer to read value from
			 * @returns number of bytes written to @c stream
			 * @throws ParseException if an error occured while writing
			 */
			template<class T>
			size_t write(std::ostream& stream, const typename T::Type& buf) throw (ParseException)
			{
				size_t bytes_written = 0, bytes_temp = 0;
				std::ostringstream err;

				// tag
				stream.put(T::Tag);
				if (!stream.good())
				{
					err << "stream was bad after writing IPND tag " << tagString<T>();
					throw ParseException(err.str(), bytes_written);
				}
				bytes_written += 1;

				// length(?) and value
				bytes_temp = Helpers<T, T::Length>::write(stream, buf);
				if (!stream.good())
				{
					err << "stream was bad after writing IPND value " << tagString<T>();
					throw ParseException(err.str(), bytes_written);
				}
				bytes_written += bytes_temp;

				return bytes_written;
			}
		}
	} // namespace net
} // namespace dtn
#endif // DISCOVERY_H_
