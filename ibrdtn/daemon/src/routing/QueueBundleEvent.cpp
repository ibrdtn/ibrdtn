/*
 * QueueBundle.cpp
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

#include "routing/QueueBundleEvent.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

namespace dtn
{
	namespace routing
	{
		QueueBundleEvent::QueueBundleEvent(const dtn::data::MetaBundle &b, const dtn::data::EID &o)
		 : bundle(b), origin(o)
		{

		}

		QueueBundleEvent::~QueueBundleEvent()
		{

		}

		void QueueBundleEvent::raise(const dtn::data::MetaBundle &bundle, const dtn::data::EID &origin)
		{
			// raise the new event
			dtn::core::EventDispatcher<QueueBundleEvent>::queue( new QueueBundleEvent(bundle, origin) );
		}

		const std::string QueueBundleEvent::getName() const
		{
			return "QueueBundleEvent";
		}

		std::string QueueBundleEvent::getMessage() const
		{
			return "New bundle queued " + bundle.toString();
		}
	}
}
