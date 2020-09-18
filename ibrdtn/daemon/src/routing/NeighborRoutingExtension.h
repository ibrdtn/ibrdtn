/*
 * NeighborRoutingExtension.h
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

#ifndef NEIGHBORROUTINGEXTENSION_H_
#define NEIGHBORROUTINGEXTENSION_H_

#include "storage/BundleResult.h"
#include "routing/RoutingExtension.h"
#include "routing/NeighborDatabase.h"
#include "core/Node.h"
#include "net/ConnectionManager.h"

#include <ibrdtn/data/MetaBundle.h>
#include "ibrdtn/data/EID.h"
#include <ibrcommon/thread/Queue.h>
#include <list>
#include <map>
#include <queue>

namespace dtn
{
	namespace routing
	{
		class NeighborRoutingExtension : public RoutingExtension, public ibrcommon::JoinableThread
		{
			static const std::string TAG;

		public:
			NeighborRoutingExtension();
			virtual ~NeighborRoutingExtension();

			virtual const std::string getTag() const noexcept;

			virtual void eventDataChanged(const dtn::data::EID &peer) noexcept;

			virtual void eventBundleQueued(const dtn::data::EID &peer, const dtn::data::MetaBundle &meta) noexcept;

			void componentUp() noexcept;
			void componentDown() noexcept;

		protected:
			void run() noexcept;
			void __cancellation() noexcept;

		private:
			class Task
			{
			public:
				virtual ~Task() {};
				virtual std::string toString() = 0;
			};

			class SearchNextBundleTask : public Task
			{
			public:
				SearchNextBundleTask(const dtn::data::EID &eid);
				virtual ~SearchNextBundleTask();

				virtual std::string toString();

				const dtn::data::EID eid;
			};

			class ProcessBundleTask : public Task
			{
			public:
				ProcessBundleTask(const dtn::data::MetaBundle &meta, const dtn::data::EID &origin, const dtn::data::EID &nexthop);
				virtual ~ProcessBundleTask();

				virtual std::string toString();

				const dtn::data::MetaBundle bundle;
				const dtn::data::EID origin;
				const dtn::data::EID nexthop;
			};

			std::pair<bool, dtn::core::Node::Protocol> shouldRouteTo(const dtn::data::MetaBundle &meta, const NeighborDatabase::NeighborEntry &n, const dtn::net::ConnectionManager::protocol_list &plist) const;

			/**
			 * hold queued tasks for later processing
			 */
			ibrcommon::Queue<NeighborRoutingExtension::Task* > _taskqueue;
		};
	}
}

#endif /* NEIGHBORROUTINGEXTENSION_H_ */
