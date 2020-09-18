/*
 * SDNV.h
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
 *    THIS FILE BASES ON DTN_2.4.0/SERVLIB/BUNDLING/SDNV.H
 */

#include <ibrcommon/Exceptions.h>
#include <ibrdtn/data/Exceptions.h>
#include <ibrdtn/utils/Random.h>

#include <sstream>
#include <sys/types.h>
#include <iostream>
#include <stdio.h>
#include <stdint.h>
#include <limits>
#include <cstdlib>

#ifndef _SDNV_H_
#define _SDNV_H_

/**
 * Class to handle parsing and formatting of self describing numeric
 * values (SDNVs).
 *
 * The basic idea is to enable a compact byte representation of
 * numeric values that may widely vary in size. This encoding is based
 * on the ASN.1 specification for encoding Object Identifier Arcs.
 *
 * Conceptually, the integer value to be encoded is split into 7-bit
 * segments. These are encoded into the output byte stream, such that
 * the high order bit in each byte is set to one for all bytes except
 * the last one.
 *
 * Note that this implementation only handles values up to 64-bits in
 * length (since the conversion is between a uint64_t and the encoded
 * byte sequence).
 */

namespace dtn
{
	namespace data
	{
		class ValueOutOfRangeException : public dtn::InvalidDataException
		{
		public:
			ValueOutOfRangeException(const std::string &what = "The value is out of range.") noexcept : dtn::InvalidDataException(what)
			{
			};
		};

		template<class E>
		class SDNV
		{
		public:
			/**
			 * The maximum length for this SDNV implementation is 10 bytes,
			 * since the maximum value is 64 bits wide.
			 */
			static const size_t MAX_LENGTH = 10;

			/**
			 * Constructor for a SDNV object
			 * @param value The new value of the SDNV
			 */
			SDNV(const E value) : _value(value) {
			}

			SDNV() : _value(0) {
			}

			/**
			 * Destructor
			 * @return
			 */
			~SDNV() {
			}

			/**
			 * Determine the encoded length of the value.
			 * @return The length of the encoded value.
			 */
			size_t getLength() const
			{
				size_t val_len = 0;
				E tmp = _value;

				do {
					tmp = tmp >> 7;
					val_len++;
				} while (tmp != 0);

				return val_len;
			}

			template<typename T>
			T get() const {
				return static_cast<T>(_value);
			}

			const E& get() const {
				return _value;
			}

			const SDNV& operator=(const E value) {
				_value = value;
				return (*this);
			}

			bool operator==(const E value) const
			{
				return (value == _value);
			}

			bool operator==(const SDNV<E> &value) const
			{
				return (value._value == _value);
			}

			bool operator!=(const E value) const
			{
				return (value != _value);
			}

			bool operator!=(const SDNV<E> &value) const
			{
				return (value._value != _value);
			}

			SDNV<E> operator+(const E value)
			{
				E result = _value + value;
				return SDNV<E>(result);
			}

			SDNV<E> operator+(const SDNV<E> &value)
			{
				E result = _value + value._value;
				return SDNV<E>(result);
			}

			friend
			SDNV<E> operator+(const SDNV<E> &left, const SDNV<E> &right)
			{
				SDNV<E> ret(left);
				ret += right;
				return ret;
			}

			friend
			SDNV<E> operator+(const SDNV<E> &left, const E right)
			{
				SDNV<E> ret(left);
				ret += right;
				return ret;
			}

			SDNV<E>& operator+=(const E value)
			{
				_value += value;
				return (*this);
			}

			SDNV<E>& operator+=(const SDNV<E> &value)
			{
				_value += value._value;
				return (*this);
			}

			SDNV<E>& operator++() // prefix
			{
				++_value;
				return (*this);
			}

			SDNV<E> operator++(int) // postfix
			{
				SDNV<E> prev(*this);
				++_value;
				return prev;
			}

			SDNV<E> operator-(const E value)
			{
				E result = _value - value;
				return SDNV<E>(result);
			}

			SDNV<E> operator-(const SDNV<E> &value)
			{
				E result = _value - value._value;
				return SDNV<E>(result);
			}

			friend
			SDNV<E> operator-(const SDNV<E> &left, const SDNV<E> &right)
			{
				SDNV<E> ret(left);
				ret -= right;
				return ret;
			}

			friend
			SDNV<E> operator-(const SDNV<E> &left, const E right)
			{
				SDNV<E> ret(left);
				ret -= right;
				return ret;
			}

			SDNV<E>& operator-=(const E value)
			{
				_value -= value;
				return (*this);
			}

			SDNV<E>& operator-=(const SDNV<E> &value)
			{
				_value -= value._value;
				return (*this);
			}

			SDNV<E>& operator--() // prefix
			{
				--_value;
				return (*this);
			}

			SDNV<E> operator--(int) // postfix
			{
				SDNV<E> prev(*this);
				--_value;
				return prev;
			}

			SDNV<E> operator/(const E value)
			{
				E result = _value / value;
				return SDNV<E>(result);
			}

			SDNV<E> operator/(const SDNV<E> &value)
			{
				E result = _value / value._value;
				return SDNV<E>(result);
			}

			friend
			SDNV<E> operator/(const SDNV<E> &left, const SDNV<E> &right)
			{
				SDNV<E> ret(left);
				ret /= right;
				return ret;
			}

			friend
			SDNV<E> operator/(const SDNV<E> &left, const E right)
			{
				SDNV<E> ret(left);
				ret /= right;
				return ret;
			}

			SDNV<E>& operator/=(const E value)
			{
				_value /= value;
				return (*this);
			}

			SDNV<E>& operator/=(const SDNV<E> &value)
			{
				_value /= value._value;
				return (*this);
			}

			SDNV<E> operator*(const E value)
			{
				E result = _value * value;
				return SDNV<E>(result);
			}

			SDNV<E> operator*(const SDNV<E> &value)
			{
				E result = _value * value._value;
				return SDNV<E>(result);
			}

			friend
			SDNV<E> operator*(const SDNV<E> &left, const SDNV<E> &right)
			{
				SDNV<E> ret(left);
				ret *= right;
				return ret;
			}

			friend
			SDNV<E> operator*(const SDNV<E> &left, const E right)
			{
				SDNV<E> ret(left);
				ret *= right;
				return ret;
			}

			SDNV<E>& operator*=(const E value)
			{
				_value *= value;
				return (*this);
			}

			SDNV<E>& operator*=(const SDNV<E> &value)
			{
				_value *= value._value;
				return (*this);
			}

			bool operator&(const SDNV<E> &value) const
			{
				return (value._value & _value);
			}

			bool operator|(const SDNV<E> &value) const
			{
				return (value._value | _value);
			}

			SDNV<E>& operator&=(const SDNV<E> &value)
			{
				_value &= value._value;
				return (*this);
			}

			SDNV<E>& operator|=(const SDNV<E> &value)
			{
				_value |= value._value;
				return (*this);
			}

			bool operator<(const E value) const
			{
				return (_value < value);
			}

			bool operator<=(const E value) const
			{
				return (_value <= value);
			}

			bool operator>(const E value) const
			{
				return (_value > value);
			}

			bool operator>=(const E value) const
			{
				return (_value >= value);
			}

			bool operator<(const SDNV<E> &value) const
			{
				return (_value < value._value);
			}

			bool operator<=(const SDNV<E> &value) const
			{
				return (_value <= value._value);
			}

			bool operator>(const SDNV<E> &value) const
			{
				return (_value > value._value);
			}

			bool operator>=(const SDNV<E> &value) const
			{
				return (_value >= value._value);
			}

			static SDNV<E> max()
			{
				return std::numeric_limits<E>::max();
			}

			template <typename V>
			SDNV<E>& random()
			{
				E val = (E)dtn::utils::Random::gen_number();
				(*this) = static_cast<E>(val);
				trim<V>();
				return (*this);
			}

			template <typename V>
			void trim()
			{
				(*this) &= std::numeric_limits<V>::max();
			}

			std::string toString() const {
				std::stringstream ss;
				ss << _value;
				return ss.str();
			}

			void fromString(const std::string &data) {
				std::stringstream ss; ss.str(data);
				ss >> _value;
			}

			void read(std::istream &stream) {
				stream >> _value;
			}

			void encode(std::ostream &stream) const
			{
				unsigned char buffer[10];
				unsigned char *bp = &buffer[0];
				uint64_t val = _value;

				const size_t val_len = getLength();

				if (!(val_len > 0)) throw ValueOutOfRangeException("ERROR(SDNV): !(val_len > 0)");
				if (!(val_len <= SDNV::MAX_LENGTH)) throw ValueOutOfRangeException("ERROR(SDNV): !(val_len <= MAX_LENGTH)");

				// Now advance bp to the last byte and fill it in backwards with the value bytes.
				bp += val_len;
				unsigned char high_bit = 0; // for the last octet
				do {
					--bp;
					*bp = (unsigned char)(high_bit | (val & 0x7f));
					high_bit = (1 << 7); // for all but the last octet
					val = val >> 7;
				} while (val != 0);

				if (!(bp == &buffer[0])) throw ValueOutOfRangeException("ERROR(SDNV): !(bp == buffer)");

				// write encoded value to the stream
				stream.write((const char*)&buffer[0], val_len);
			}

			void decode(std::istream &stream)
			{
				size_t val_len = 0;
				unsigned char bp = 0;
				unsigned char start = 0;

				int carry = 0;

				_value = 0;
				do {
					stream.get((char&)bp);

					_value = (_value << 7) | (bp & 0x7f);
					++val_len;

					if ((bp & (1 << 7)) == 0)
						break; // all done;

					// check if the value fits into sizeof(E)
					if ((val_len % 8) == 0) ++carry;

					if ((sizeof(E) + carry) < val_len)
						throw ValueOutOfRangeException("ERROR(SDNV): overflow value in sdnv");

					if (start == 0) start = bp;
				} while (1);

				if ((val_len > SDNV::MAX_LENGTH) || ((val_len == SDNV::MAX_LENGTH) && (start != 0x81)))
					throw ValueOutOfRangeException("ERROR(SDNV): overflow value in sdnv");
			}

		private:
			friend
			std::ostream &operator<<(std::ostream &stream, const dtn::data::SDNV<E> &obj)
			{
				obj.encode(stream);
				return stream;
			}

			friend
			std::istream &operator>>(std::istream &stream, dtn::data::SDNV<E> &obj)
			{
				obj.decode(stream);
				return stream;
			}

			E _value;
		};
	}
}

#endif /* _SDNV_H_ */
