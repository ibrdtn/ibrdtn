/* 
 * Serializer.h
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

#ifndef _SERIALIZER_H
#define	_SERIALIZER_H

#include <iostream>
#include "ibrdtn/data/Dictionary.h"
#include "ibrdtn/data/PrimaryBlock.h"
#include "ibrdtn/data/Exceptions.h"
#include "ibrdtn/data/BundleFragment.h"

namespace dtn
{
	namespace data
	{
		class Bundle;
		class Block;
		class PrimaryBlock;
		class PayloadBlock;
		class MetaBundle;

		class Serializer
		{
		public:
			virtual ~Serializer() {};

			virtual Serializer &operator<<(const dtn::data::Bundle &obj) = 0;
			virtual Serializer &operator<<(const dtn::data::BundleFragment &obj)
			{
				(*this) << obj._bundle;
				return (*this);
			};

			virtual Length getLength(const dtn::data::Bundle &obj) = 0;
		};

		class Deserializer
		{
		public:
			virtual ~Deserializer() {};

			virtual Deserializer &operator>>(dtn::data::Bundle &obj) = 0;
		};

		class Validator
		{
		public:
			class RejectedException : public dtn::SerializationFailedException
			{
			public:
				RejectedException(std::string what = "A validate method has the bundle rejected.") throw() : dtn::SerializationFailedException(what)
				{
				};
			};

			virtual ~Validator() {};

			virtual void validate(const dtn::data::PrimaryBlock&) const throw (RejectedException) = 0;
			virtual void validate(const dtn::data::Block&, const dtn::data::Number&) const throw (RejectedException) = 0;
			virtual void validate(const dtn::data::PrimaryBlock&, const dtn::data::Block&, const dtn::data::Number&) const throw (RejectedException) = 0;
			virtual void validate(const dtn::data::Bundle&) const throw (RejectedException) = 0;
		};

		class AcceptValidator : public Validator
		{
		public:
			AcceptValidator();
			virtual ~AcceptValidator();

			virtual void validate(const dtn::data::PrimaryBlock&) const throw (RejectedException);
			virtual void validate(const dtn::data::Block&, const dtn::data::Number&) const throw (RejectedException);
			virtual void validate(const dtn::data::PrimaryBlock&, const dtn::data::Block&, const dtn::data::Number&) const throw (RejectedException);
			virtual void validate(const dtn::data::Bundle&) const throw (RejectedException);
		};

		class DefaultSerializer : public Serializer
		{
		public:
			/**
			 * Default serializer.
			 * @param stream Stream to write to
			 */
			DefaultSerializer(std::ostream &stream);

			/**
			 * Initialize the Serializer with a default dictionary. This will be used to write the
			 * right values in the EID reference part of blocks.
			 * @param stream Stream to write to
			 * @param d The default dictionary
			 */
			DefaultSerializer(std::ostream &stream, const Dictionary &d);

			/**
			 * Destructor
			 */
			virtual ~DefaultSerializer() {};

			virtual Serializer &operator<<(const dtn::data::Bundle &obj);
			virtual Serializer &operator<<(const dtn::data::PrimaryBlock &obj);
			virtual Serializer &operator<<(const dtn::data::Block &obj);
			virtual Serializer &operator<<(const dtn::data::BundleFragment &obj);

			virtual Length getLength(const dtn::data::Bundle &obj);
			virtual Length getLength(const dtn::data::PrimaryBlock &obj) const;
			virtual Length getLength(const dtn::data::Block &obj) const;

		protected:
			Serializer &serialize(const dtn::data::PayloadBlock& obj, const Length &clip_offset, const Length &clip_length);
			void rebuildDictionary(const dtn::data::Bundle &obj);
			bool isCompressable(const dtn::data::Bundle &obj) const;
			std::ostream &_stream;

			Dictionary _dictionary;
			bool _compressable;
		};

		class DefaultDeserializer : public Deserializer
		{
		public:
			/**
			 * Default Deserializer
			 * @param stream Stream to read from
			 */
			DefaultDeserializer(std::istream &stream);

			/**
			 * Initialize the Deserializer
			 * The validator can check each block, header or bundle for validity.
			 * @param stream Stream to read from
			 * @param v Validator for the bundles and blocks
			 * @return
			 */
			DefaultDeserializer(std::istream &stream, Validator &v);

			/**
			 * Default destructor.
			 * @return
			 */
			virtual ~DefaultDeserializer() {};

			virtual Deserializer &operator>>(dtn::data::Bundle &obj);
			virtual Deserializer &operator>>(dtn::data::PrimaryBlock &obj);
			virtual Deserializer &operator>>(dtn::data::Block &obj);
			virtual Deserializer &read(const dtn::data::PrimaryBlock &bundle, dtn::data::Block &obj);
			virtual Deserializer &operator>>(dtn::data::MetaBundle &obj);

			/**
			 * Enable or disable reactive fragmentation support.
			 * (Default is disabled.)
			 * @param val
			 */
			void setFragmentationSupport(bool val);

		protected:
			std::istream &_stream;
			Validator &_validator;
			AcceptValidator _default_validator;

		private:
			Dictionary _dictionary;
			bool _compressed;
			bool _fragmentation;
		};

		class SeparateSerializer : public DefaultSerializer
		{
		public:
			SeparateSerializer(std::ostream &stream);
			virtual ~SeparateSerializer();

			virtual Serializer &operator<<(const dtn::data::Block &obj);
			virtual Length getLength(const dtn::data::Block &obj) const;
		};

		class SeparateDeserializer : public DefaultDeserializer
		{
		public:
			SeparateDeserializer(std::istream &stream, Bundle &b);
			virtual ~SeparateDeserializer();

			dtn::data::Block& readBlock();

		private:
			Bundle &_bundle;
		};
	}
}


#endif	/* _SERIALIZER_H */

