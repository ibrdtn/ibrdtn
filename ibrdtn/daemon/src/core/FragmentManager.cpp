/*
 * FragmentManager.cpp
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

#include "Configuration.h"
#include "core/EventDispatcher.h"
#include "core/FragmentManager.h"
#include "core/BundleCore.h"
#include "core/BundlePurgeEvent.h"
#include "net/BundleReceivedEvent.h"

#include <ibrdtn/data/BundleMerger.h>
#include <ibrdtn/utils/Clock.h>
#include <ibrcommon/Logger.h>
#include <ibrcommon/thread/MutexLock.h>

#include <ibrdtn/ibrdtn.h>
#ifdef IBRDTN_SUPPORT_BSP
#include "security/SecurityManager.h"
#endif

namespace dtn
{
	namespace core
	{
		const std::string FragmentManager::TAG = "FragmentManager";

		ibrcommon::Mutex FragmentManager::_offsets_mutex;
		std::set<FragmentManager::Transmission> FragmentManager::_offsets;

		FragmentManager::FragmentManager()
		 : _running(false)
		{
		}

		FragmentManager::~FragmentManager()
		{
		}

		const std::string FragmentManager::getName() const
		{
			return "FragmentManager";
		}

		void FragmentManager::__cancellation() noexcept
		{
			_running = false;
			_incoming.abort();
		}

		void FragmentManager::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::add(this);
			_running = true;
		}

		void FragmentManager::componentRun() noexcept
		{
			// get reference to the storage
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();

			// TODO: scan storage for fragments to reassemble on startup

			dtn::storage::BundleResultList list;

			// create a task loop to reassemble fragments asynchronously
			try {
				while (_running)
				{
					dtn::data::MetaBundle meta = _incoming.poll();

					// skip merge if complete bundle is already in the storage
					dtn::data::BundleID origin(meta);
					origin.setFragment(false);
					if (storage.contains(origin)) continue;

					// search for matching bundles
					list.clear();
					search(meta, list);

					IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 20) << "found " << list.size() << " fragments similar to bundle " << meta.toString() << IBRCOMMON_LOGGER_ENDL;

					// TODO: drop fragments if other fragments available containing the same payload or larger payload

					// check first if all fragment are available
					std::set<BundleMerger::Chunk> chunks;
					for (std::list<dtn::data::MetaBundle>::const_iterator iter = list.begin(); iter != list.end(); ++iter)
					{
						const dtn::data::MetaBundle &m = (*iter);
						if (meta.getPayloadLength() > 0)
						{
							BundleMerger::Chunk chunk(m.fragmentoffset.get<dtn::data::Length>(), m.getPayloadLength());
							chunks.insert(chunk);
						}
					}

					// wait for the next bundle if the fragment is not complete
					if (!BundleMerger::Chunk::isComplete(meta.appdatalength.get<dtn::data::Length>(), chunks)) continue;

					// create a new bundle merger container
					dtn::data::BundleMerger::Container c = dtn::data::BundleMerger::getContainer();

					for (std::list<dtn::data::MetaBundle>::const_iterator iter = list.begin(); iter != list.end(); ++iter)
					{
						const dtn::data::MetaBundle &meta = (*iter);

						if (meta.getPayloadLength() > 0)
						{
							IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 20) << "fragment: " << (*iter).toString() << IBRCOMMON_LOGGER_ENDL;

							try {
								// load bundle from storage
								const dtn::data::Bundle bundle = dtn::core::BundleCore::getInstance().getStorage().get(*iter);

								// merge the bundle
								c << bundle;
							} catch (const dtn::storage::NoBundleFoundException&) {
								IBRCOMMON_LOGGER_TAG(FragmentManager::TAG, error) << "could not load fragment to merge bundle" << IBRCOMMON_LOGGER_ENDL;
							};
						}
					}

					if (c.isComplete())
					{
						dtn::data::Bundle &merged = c.getBundle();

						IBRCOMMON_LOGGER_TAG(FragmentManager::TAG, notice) << "Bundle " << merged.toString() << " merged" << IBRCOMMON_LOGGER_ENDL;

						// pass merged bundle through the filter
						FilterContext context;
						context.setBundle(merged);
						if (BundleCore::getInstance().filter(BundleFilter::INPUT, context, merged) == BundleFilter::ACCEPT)
						{
							// raise default bundle received event
							dtn::net::BundleReceivedEvent::raise(dtn::core::BundleCore::local, merged, true);
						}

						// delete all fragments of the merged bundle
						if (merged.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
						{
							for (std::list<dtn::data::MetaBundle>::const_iterator iter = list.begin(); iter != list.end(); ++iter)
							{
								dtn::core::BundlePurgeEvent::raise(*iter);
							}
						}
					}
				}
			} catch (const ibrcommon::QueueUnblockedException&) { }
		}

		void FragmentManager::componentDown() noexcept
		{
			dtn::core::EventDispatcher<dtn::routing::QueueBundleEvent>::remove(this);

			stop();
			join();
		}

		void FragmentManager::raiseEvent(const dtn::routing::QueueBundleEvent &queued) noexcept
		{
			// process fragments
			if (!queued.bundle.isFragment()) return;

			// do not merge a bundle if it is non-local and singleton
			// we only touch local and group bundles which might be delivered locally
			if (queued.bundle.get(dtn::data::PrimaryBlock::DESTINATION_IS_SINGLETON))
			{
				if (!queued.bundle.destination.sameHost(dtn::core::BundleCore::local))
				{
					return;
				}
			}

			// push the meta bundle into the incoming queue
			_incoming.push(queued.bundle);
		}

		void FragmentManager::search(const dtn::data::MetaBundle &meta, dtn::storage::BundleResult &list)
		{
			class BundleFilter : public dtn::storage::BundleSelector
			{
			public:
				BundleFilter(const dtn::data::MetaBundle &meta)
				 : _similar(meta)
				{};

				virtual ~BundleFilter() {};

				virtual dtn::data::Size limit() const noexcept { return 0; };

				virtual bool shouldAdd(const dtn::data::MetaBundle &meta) const noexcept (false)
				{
					// fragments only
					if (!meta.isFragment()) return false;

					// with the same unique bundle id
					if (meta.source != _similar.source) return false;
					if (meta.timestamp != _similar.timestamp) return false;
					if (meta.sequencenumber != _similar.sequencenumber) return false;

					return true;
				};

			private:
				const dtn::data::MetaBundle &_similar;
			};

			// create a bundle filter
			BundleFilter filter(meta);
			dtn::storage::BundleStorage &storage = dtn::core::BundleCore::getInstance().getStorage();
			
			try {
				storage.get(filter, list);
			} catch (const dtn::storage::NoBundleFoundException&) { }
		}

		void FragmentManager::setOffset(const dtn::data::EID &peer, const dtn::data::BundleID &id, const dtn::data::Length &abs_offset, const dtn::data::Length &frag_offset) noexcept
		{
			try {
				Transmission t;
				dtn::data::Bundle b = dtn::core::BundleCore::getInstance().getStorage().get(id);
				t.offset = get_payload_offset(b, abs_offset, frag_offset);

				if (t.offset <= 0) return;

				t.id = id;
				t.peer = peer;
				t.expires = dtn::utils::Clock::getExpireTime( b );

				IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 4) << "Store offset of partial transmitted bundle " << id.toString() << " to " << peer.getString() <<
						", offset: " << t.offset << " (" << abs_offset << ")" << IBRCOMMON_LOGGER_ENDL;

				ibrcommon::MutexLock l(_offsets_mutex);
				_offsets.erase(t);
				_offsets.insert(t);
			} catch (const dtn::storage::NoBundleFoundException&) { };
		}

		dtn::data::Length FragmentManager::getOffset(const dtn::data::EID &peer, const dtn::data::BundleID &id) noexcept
		{
			ibrcommon::MutexLock l(_offsets_mutex);
			for (std::set<Transmission>::const_iterator iter = _offsets.begin(); iter != _offsets.end(); ++iter)
			{
				const Transmission &t = (*iter);
				if (t.peer != peer) continue;
				if (t.id != id) continue;
				return t.offset;
			}

			return 0;
		}

		dtn::data::Length FragmentManager::get_payload_offset(const dtn::data::Bundle &bundle, const dtn::data::Length &abs_offset, const dtn::data::Length &frag_offset) noexcept
		{
			try {
				// build the dictionary for EID lookup
				const dtn::data::Dictionary dict(bundle);

				PrimaryBlock prim = bundle;

				if (frag_offset > 0) {
					prim.fragmentoffset += frag_offset;
					if (!prim.get(dtn::data::PrimaryBlock::FRAGMENT)) {
						prim.appdatalength = bundle.find<dtn::data::PayloadBlock>().getLength();
						prim.set(dtn::data::PrimaryBlock::FRAGMENT, true);
					}
				}

				// create a default serializer
				dtn::data::DefaultSerializer serializer(std::cout, dict);

				// get the encoded length of the primary block
				dtn::data::Length header = serializer.getLength(prim);

				for (dtn::data::Bundle::const_iterator iter = bundle.begin(); iter != bundle.end(); ++iter)
				{
					const dtn::data::Block &b = (**iter);
					header += serializer.getLength(b);

					try {
						const dtn::data::PayloadBlock &payload = dynamic_cast<const dtn::data::PayloadBlock&>(b);
						header -= payload.getLength();
						if (abs_offset < header) return 0;
						return frag_offset + (abs_offset - header);
					} catch (std::bad_cast&) { };
				}
			} catch (const dtn::InvalidDataException&) {
				// failure while calculating the bundle length
			}

			return 0;
		}

		void FragmentManager::expire_offsets(const dtn::data::Timestamp &timestamp)
		{
			ibrcommon::MutexLock l(_offsets_mutex);
			for (std::set<Transmission>::iterator iter = _offsets.begin(); iter != _offsets.end();)
			{
				const Transmission &t = (*iter);
				if (t.expires >= timestamp) return;
				_offsets.erase(iter++);
			}
		}

		void FragmentManager::split(const dtn::data::Bundle &bundle, const dtn::data::Length &maxPayloadLength, std::list<dtn::data::Bundle> &fragments) noexcept (false)
		{
			// get bundle DONT_FRAGMENT Flag
			if (bundle.get(dtn::data::PrimaryBlock::DONT_FRAGMENT))
				throw FragmentationProhibitedException("Bundle fragmentation is restricted by do-not-fragment bit.");

			try {
				const dtn::data::PayloadBlock &payloadBlock = bundle.find<dtn::data::PayloadBlock>();

				// get bundle payload length
				dtn::data::Length payloadLength = payloadBlock.getLength();

				// check if fragmentation needed
				if (payloadLength <= maxPayloadLength)
					throw FragmentationNotNecessaryException("Fragmentation not necessary. The payload block is smaller than the max. payload length.");

				// copy the origin bundle as template for the new fragment
				Bundle fragment = bundle;

				// clear all the blocks
				fragment.clear();

				// set bundle is fragment flag
				fragment.set(dtn::data::PrimaryBlock::FRAGMENT, true);

				// set application data length
				fragment.appdatalength = payloadLength;

				ibrcommon::BLOB::Reference ref = payloadBlock.getBLOB();
				ibrcommon::BLOB::iostream stream = ref.iostream();

				bool isFirstFragment = true;
				dtn::data::Length offset = 0;

				while (!(*stream).eof() && (payloadLength > offset))
				{
					// clear all the blocks
					fragment.clear();

					// set fragment offset
					fragment.fragmentoffset = offset;

					// copy partial payload to the payload of the fragment
					try {
						// create new blob for fragment payload
						ibrcommon::BLOB::Reference fragment_ref = ibrcommon::BLOB::create();

						// get the iostream object
						ibrcommon::BLOB::iostream fragment_stream = fragment_ref.iostream();

						if ((offset + maxPayloadLength) > payloadLength) {
							// copy payload to the fragment
							ibrcommon::BLOB::copy(*fragment_stream, *stream, payloadLength - offset, 65535);
						} else {
							// copy payload to the fragment
							ibrcommon::BLOB::copy(*fragment_stream, *stream, maxPayloadLength, 65535);
						}

						// set new offset position
						offset += fragment_stream.size();

						// create fragment payload block
						dtn::data::PayloadBlock &fragment_payloadBlock = fragment.push_back(fragment_ref);

						// add all necessary blocks from the bundle to the fragment
						addBlocksFromBundleToFragment(bundle, fragment, fragment_payloadBlock, isFirstFragment, payloadLength == offset);
					} catch (const ibrcommon::IOException &ex) {
						IBRCOMMON_LOGGER_TAG(FragmentManager::TAG, error) << "error while splitting bundle into fragments: " << ex.what() << IBRCOMMON_LOGGER_ENDL;
					}

					// add current fragment to fragments list
					fragments.push_back(fragment);

					if (isFirstFragment) isFirstFragment = false;

					IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 5) << "Fragment created: " << fragment.toString() << IBRCOMMON_LOGGER_ENDL;
				}
			} catch (const dtn::data::Bundle::NoSuchBlockFoundException&) {
				// bundle has no payload
				throw FragmentationAbortedException("Fragmentation aborted. No payload block found.");
			}
		}

		void FragmentManager::addBlocksFromBundleToFragment(const dtn::data::Bundle &bundle, dtn::data::Bundle &fragment, dtn::data::PayloadBlock &fragment_payloadBlock, bool isFirstFragment, bool isLastFragment)
		{
			bool isAfterPayload = false;
			bool isReplicateInEveryBundle = false;

			char block_type = 0;

			IBRCOMMON_LOGGER_DEBUG_TAG("FragmentManager", 5) << "Fragment original bundle block count: " << fragment.toString() << "  " << bundle.size() << IBRCOMMON_LOGGER_ENDL;

			//check for each block if it has to be added to the fragment
			for (dtn::data::Bundle::const_iterator it = bundle.begin(); it != bundle.end(); ++it)
			{
				//get the current block
				const Block &current_block = dynamic_cast<const Block&>(**it);

				block_type = current_block.getType();

				IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 5) << "Fragment block found: " << fragment.toString() << "  " << (unsigned int)block_type << IBRCOMMON_LOGGER_ENDL;


				if (block_type == dtn::data::PayloadBlock::BLOCK_TYPE)
				{
					isAfterPayload = true;
				}
				else
				{
					isReplicateInEveryBundle = current_block.get(dtn::data::Block::REPLICATE_IN_EVERY_FRAGMENT);

					//if block is before payload
					//add if fragment is the first one
					//or if ReplicateInEveryBundle Flag is set
					if (!isAfterPayload && (isFirstFragment || isReplicateInEveryBundle))
					{
						try
						{	//get factory
							ExtensionBlock::Factory &f = dtn::data::ExtensionBlock::Factory::get(block_type);

							//insert new Block before payload block
							dtn::data::Block &fragment_block = fragment.insert(fragment.find(fragment_payloadBlock), f);

							//copy block
							fragment_block = current_block;

							IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 5) << "Added block before payload: " << fragment.toString()<< "  " << block_type << IBRCOMMON_LOGGER_ENDL;
						}
						catch(const ibrcommon::Exception &ex)
						{
							//insert new Block before payload block
							dtn::data::Block &fragment_block = fragment.insert<dtn::data::ExtensionBlock>(fragment.find(fragment_payloadBlock));

							//copy block
							fragment_block = current_block;

							IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 5) << "Added block before payload: " << fragment.toString()<< "  " << block_type << IBRCOMMON_LOGGER_ENDL;
						}

					}
					//if block is after payload
					//add if fragment is the last one
					//or if ReplicateInEveryBundle Flag is set
					else if (isAfterPayload && (isLastFragment || isReplicateInEveryBundle))
					{
						try
						{	//get factory
							ExtensionBlock::Factory &f = dtn::data::ExtensionBlock::Factory::get(block_type);

							//push back new Block after payload block
							dtn::data::Block &fragment_block = fragment.push_back(f);

							//copy block
							fragment_block = current_block;

							IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 5) << "Added block after payload: " << fragment.toString()<< "  " << block_type << IBRCOMMON_LOGGER_ENDL;
						}
						catch (const ibrcommon::Exception &ex)
						{
							//push back new Block after payload block
							dtn::data::Block &fragment_block = fragment.push_back<dtn::data::ExtensionBlock>();

							//copy block
							fragment_block = current_block;

							IBRCOMMON_LOGGER_DEBUG_TAG(FragmentManager::TAG, 5) << "Added block after payload: " << fragment.toString()<< "  " << block_type << IBRCOMMON_LOGGER_ENDL;
						}
					}
				}
			}

		}

		FragmentManager::Transmission::Transmission()
		 : offset(0), expires(0)
		{
		}

		FragmentManager::Transmission::~Transmission()
		{
		}

		bool FragmentManager::Transmission::operator<(const Transmission &other) const
		{
			if (expires < other.expires) return true;
			if (expires != other.expires) return false;

			if (peer < other.peer) return true;
			if (peer != other.peer) return false;

			return (id < other.id);
		}

		bool FragmentManager::Transmission::operator==(const Transmission &other) const
		{
			if (expires != other.expires) return false;
			if (peer != other.peer) return false;
			if (id != other.id) return false;

			return true;
		}
	} /* namespace core */
} /* namespace dtn */
