/*
 * FragmentManager.h
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

#ifndef FRAGMENTMANAGER_H_
#define FRAGMENTMANAGER_H_

#include "Component.h"
#include "core/EventReceiver.h"
#include "storage/BundleResult.h"
#include "routing/QueueBundleEvent.h"
#include <ibrdtn/data/MetaBundle.h>
#include <ibrcommon/thread/Queue.h>
#include <ibrcommon/thread/Mutex.h>
#include <list>
#include <set>

namespace dtn
{
	namespace core
	{
		class FragmentationAbortedException : public ibrcommon::Exception
		{
		public:
			FragmentationAbortedException(std::string what = "Fragmentation aborted.") throw() : ibrcommon::Exception(what)
			{
			}
		};

		class FragmentationProhibitedException : public FragmentationAbortedException
		{
		public:
			FragmentationProhibitedException(std::string what = "Fragmentation is prohibited.") throw() : FragmentationAbortedException(what)
			{
			}
		};

		class FragmentationNotNecessaryException : public FragmentationAbortedException
		{
		public:
			FragmentationNotNecessaryException(std::string what = "Fragmentation is not necessary.") throw() : FragmentationAbortedException(what)
			{
			}
		};

		class FragmentManager : public dtn::daemon::IndependentComponent, public dtn::core::EventReceiver<dtn::routing::QueueBundleEvent>
		{
			static const std::string TAG;

		public:
			FragmentManager();
			virtual ~FragmentManager();

			void __cancellation() throw ();

			void componentUp() throw ();
			void componentRun() throw ();
			void componentDown() throw ();

			void raiseEvent(const dtn::routing::QueueBundleEvent &evt) throw ();

			const std::string getName() const;

			/**
			 * Updates the offset of a transmission
			 * @param peer
			 * @param id
			 * @param offset
			 */
			static void setOffset(const dtn::data::EID &peer, const dtn::data::BundleID &id, const dtn::data::Length &abs_offset, const dtn::data::Length &frag_offset) throw ();

			/**
			 * Get the offset of a transmission
			 * @param peer
			 * @param id
			 * @return
			 */
			static dtn::data::Length getOffset(const dtn::data::EID &peer, const dtn::data::BundleID &id) throw ();

			/**
			 * Split-up a bundle into several pieces
			 * @param bundle the original bundle
			 * @param maxPayloadLength payload length maximum per fragment
			 * @param fragments list of all fragments
			 */
			static void split(const dtn::data::Bundle &bundle, const dtn::data::Length &maxPayloadLength, std::list<dtn::data::Bundle> &fragments) throw (FragmentationAbortedException);

		private:
			class Transmission
			{
			public:
				Transmission();
				virtual ~Transmission();

				bool operator<(const Transmission &other) const;
				bool operator==(const Transmission &other) const;

				dtn::data::EID peer;
				dtn::data::BundleID id;
				dtn::data::Length offset;
				dtn::data::Timestamp expires;
			};

			static void expire_offsets(const dtn::data::Timestamp &timestamp);
			static dtn::data::Length get_payload_offset(const dtn::data::Bundle &bundle, const dtn::data::Length &abs_offset, const dtn::data::Length &frag_offset) throw ();

			/**
			 * adds all necessary blocks from the bundle to the fragment
			 * @param bundle the original bundle
			 * @param fragment the current fragment
			 * @param fragment_payloadBlock payload block of the current fragment
			 * @param isFirstFragment
			 * @param isLastFragment
			 */
			static void addBlocksFromBundleToFragment(const dtn::data::Bundle &bundle, dtn::data::Bundle &fragment, dtn::data::PayloadBlock &fragment_payloadBlock, bool isFirstFragment, bool isLastFragment);

			// search for similar bundles in the storage
			void search(const dtn::data::MetaBundle &meta, dtn::storage::BundleResult &list);

			ibrcommon::Queue<dtn::data::MetaBundle> _incoming;
			bool _running;

			static ibrcommon::Mutex _offsets_mutex;
			static std::set<Transmission> _offsets;
		};
	} /* namespace core */
} /* namespace dtn */
#endif /* FRAGMENTMANAGER_H_ */
