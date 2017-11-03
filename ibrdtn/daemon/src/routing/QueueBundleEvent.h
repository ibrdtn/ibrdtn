/*
 * QueueBundle.h
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

#ifndef QUEUEBUNDLEEVENT_H_
#define QUEUEBUNDLEEVENT_H_

#include "core/Event.h"
#include "ibrdtn/data/Bundle.h"
#include "ibrdtn/data/MetaBundle.h"
#include "ibrdtn/data/EID.h"

namespace dtn
{
	namespace routing
	{
		class QueueBundleEvent : public dtn::core::Event
		{
		public:
			virtual ~QueueBundleEvent();

			const std::string getName() const;

			std::string getMessage() const;

			static void raise(const dtn::data::MetaBundle &bundle, const dtn::data::EID &origin);

			const dtn::data::MetaBundle bundle;
			const dtn::data::EID origin;

		private:
			QueueBundleEvent(const dtn::data::MetaBundle &bundle, const dtn::data::EID &origin);
		};
	}
}

#endif /* QUEUEBUNDLEEVENT_H_ */
