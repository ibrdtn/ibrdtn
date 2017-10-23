/*
 * TransferCompletedEvent.h
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

#ifndef TRANSFERCOMPLETEDEVENT_H_
#define TRANSFERCOMPLETEDEVENT_H_

#include "core/Event.h"
#include <ibrdtn/data/MetaBundle.h>
#include "ibrdtn/data/EID.h"

namespace dtn
{
	namespace net
	{
		class TransferCompletedEvent : public dtn::core::Event
		{
		public:
			virtual ~TransferCompletedEvent();

			const std::string getName() const;

			std::string getMessage() const;

			static void raise(const dtn::data::EID peer, const dtn::data::MetaBundle &bundle);

			const dtn::data::EID& getPeer() const;
			const dtn::data::MetaBundle& getBundle() const;

		private:
			dtn::data::EID _peer;
			dtn::data::MetaBundle _bundle;
			TransferCompletedEvent(const dtn::data::EID peer, const dtn::data::MetaBundle &bundle);
		};
	}
}

#endif /* TRANSFERCOMPLETEDEVENT_H_ */
