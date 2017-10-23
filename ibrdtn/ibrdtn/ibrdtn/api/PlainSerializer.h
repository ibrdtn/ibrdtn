/*
 * PlainSerializer.h
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

#ifndef PLAINSERIALIZER_H_
#define PLAINSERIALIZER_H_

#include <ibrdtn/data/Serializer.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/PrimaryBlock.h>
#include <ibrdtn/data/Block.h>

namespace dtn
{
	namespace api
	{
		class PlainSerializer : public dtn::data::Serializer
		{
		public:
			enum Encoding {
				INVALID,
				SKIP_PAYLOAD,
				BASE64,
				RAW
			};

			static Encoding parseEncoding(const std::string &data);
			static std::string printEncoding(const Encoding &enc);

			PlainSerializer(std::ostream &stream, Encoding enc = BASE64);
			virtual ~PlainSerializer();

			dtn::data::Serializer &operator<<(const dtn::data::Bundle &obj);
			dtn::data::Serializer &operator<<(const dtn::data::PrimaryBlock &obj);
			dtn::data::Serializer &operator<<(const dtn::data::Block &obj);

			void writeData(const dtn::data::Block &block);
			void writeData(std::istream &stream, const dtn::data::Length &len);

			dtn::data::Length getLength(const dtn::data::Bundle &obj);
			dtn::data::Length getLength(const dtn::data::PrimaryBlock &obj) const;
			dtn::data::Length getLength(const dtn::data::Block &obj) const;

		private:
			std::ostream &_stream;
			Encoding _encoding;
		};

		class PlainDeserializer : public dtn::data::Deserializer
		{
		public:
			class UnknownBlockException;
			class BlockNotProcessableException;

			PlainDeserializer(std::istream &stream);
			virtual ~PlainDeserializer();

			dtn::data::Deserializer &operator>>(dtn::data::Bundle &obj);
			dtn::data::Deserializer &operator>>(dtn::data::PrimaryBlock &obj);
			dtn::data::Deserializer &operator>>(dtn::data::Block &obj);
			dtn::data::Deserializer &operator>>(ibrcommon::BLOB::iostream &obj);

			/**
			 * read a block from _stream and add it to a bundle using a given BlockInserter
			 * @param builder Use this builder to insert the block
			 * @return the block that was inserted
			 */
			dtn::data::Block& readBlock(dtn::data::BundleBuilder &builder);

			/**
			 * deserialize a base64 encoded data stream into another stream
			 */
			void readData(std::ostream &stream);

		private:
			std::istream &_stream;
			bool _lastblock;

		public:
			class PlainDeserializerException : public ibrcommon::Exception
			{
				public:
					PlainDeserializerException(std::string what = "") throw() : Exception(what)
					{
					}
			};

			class UnknownBlockException : public PlainDeserializerException
			{
				public:
					UnknownBlockException(std::string what = "unknown block") throw() : PlainDeserializerException(what)
					{
					}
			};

			class BlockNotProcessableException : public PlainDeserializerException
			{
				public:
					BlockNotProcessableException(std::string what = "block not processable") throw() : PlainDeserializerException(what)
					{
					}
			};

		};
	}
}

#endif /* PLAINSERIALIZER_H_ */
