/*
 * BundleBuilder.h
 *
 *  Created on: 10.01.2013
 *      Author: morgenro
 */

#ifndef BUNDLEBUILDER_H_
#define BUNDLEBUILDER_H_

#include <ibrdtn/data/Number.h>
#include <ibrdtn/data/Bundle.h>
#include <ibrdtn/data/Block.h>
#include <iterator>
#include <stdlib.h>

namespace dtn
{
	namespace data
	{
		class BundleBuilder {
		public:
			class DiscardBlockException : public dtn::SerializationFailedException
			{
			public:
				DiscardBlockException(std::string what = "Block has been discarded.") throw() : dtn::SerializationFailedException(what)
				{
				};
			};

			enum POSITION
			{
				FRONT,
				MIDDLE,
				END
			};

			BundleBuilder(Bundle &target, POSITION alignment = END, int pos = 0);
			virtual ~BundleBuilder();

			/**
			 * clear the content of the target bundle
			 */
			void clear();

			template <class T>
			T& insert(const Bitset<Block::ProcFlags> &procflags);

			POSITION getAlignment() const;

			dtn::data::Block &insert(dtn::data::ExtensionBlock::Factory &f, const Bitset<Block::ProcFlags> &procflags);

			/**
			 * Add a block to the bundle.
			 */
			dtn::data::Block& insert(dtn::data::block_t block_type, const Bitset<Block::ProcFlags> &procflags);

		private:
			Bundle *_target;

			POSITION _alignment;
			int _pos;
		};

		template <class T>
		T& BundleBuilder::insert(const Bitset<Block::ProcFlags> &procflags)
		{
			switch (_alignment)
			{
			case FRONT:
			{
				T &block = _target->push_front<T>();
				bool last_block = block.get(dtn::data::Block::LAST_BLOCK);
				block._procflags = procflags;
				block.set(dtn::data::Block::LAST_BLOCK, last_block);
				return block;
			}

			case END:
			{
				T &block = _target->push_back<T>();
				bool last_block = block.get(dtn::data::Block::LAST_BLOCK);
				block._procflags = procflags;
				block.set(dtn::data::Block::LAST_BLOCK, last_block);
				return block;
			}

			default:
				if(_pos <= 0) {
					T &block = _target->push_front<T>();
					bool last_block = block.get(dtn::data::Block::LAST_BLOCK);
					block._procflags = procflags;
					block.set(dtn::data::Block::LAST_BLOCK, last_block);
					return block;
				}

				dtn::data::Bundle::iterator it = _target->begin();
				std::advance(it, _pos-1);

				T &block = (it == _target->end()) ? _target->push_back<T>() : _target->insert<T>(it);

				bool last_block = block.get(dtn::data::Block::LAST_BLOCK);
				block._procflags = procflags;
				block.set(dtn::data::Block::LAST_BLOCK, last_block);
				return block;
			}
		}
	} /* namespace data */
} /* namespace dtn */
#endif /* BUNDLEBUILDER_H_ */
