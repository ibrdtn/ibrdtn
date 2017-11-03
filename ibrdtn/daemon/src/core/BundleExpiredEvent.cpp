/*
 * BundleExpiredEvent.cpp
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

#include "core/BundleExpiredEvent.h"
#include "core/BundleCore.h"
#include "core/EventDispatcher.h"

namespace dtn
{
	namespace core
	{
		BundleExpiredEvent::BundleExpiredEvent(const dtn::data::BundleID &bundle)
		 : _bundle(bundle)
		{

		}

		BundleExpiredEvent::BundleExpiredEvent(const dtn::data::Bundle &bundle)
		 : _bundle(bundle)
		{

		}

		BundleExpiredEvent::~BundleExpiredEvent()
		{

		}

		void BundleExpiredEvent::raise(const dtn::data::Bundle &bundle)
		{
			// raise the new event
			dtn::core::EventDispatcher<BundleExpiredEvent>::queue( new BundleExpiredEvent(bundle) );
		}

		void BundleExpiredEvent::raise(const dtn::data::BundleID &bundle)
		{
			// raise the new event
			dtn::core::EventDispatcher<BundleExpiredEvent>::queue( new BundleExpiredEvent(bundle) );
		}

		const std::string BundleExpiredEvent::getName() const
		{
			return "BundleExpiredEvent";
		}

		std::string BundleExpiredEvent::getMessage() const
		{
			return "Bundle has been expired " + _bundle.toString();
		}

		const dtn::data::BundleID& BundleExpiredEvent::getBundle() const
		{
			return _bundle;
		}
	}
}
