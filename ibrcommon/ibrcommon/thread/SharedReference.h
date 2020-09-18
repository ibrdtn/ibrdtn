/*
 * SharedReference.h
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

#ifndef SHAREDREFERENCE_H_
#define SHAREDREFERENCE_H_

#include "ibrcommon/thread/RWMutex.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/Exceptions.h"

namespace ibrcommon {

	template <class T>
	class SharedReference
	{
	public:
		SharedReference(T* ref, RWMutex& mutex)
			: _item(ref), _l(mutex)
		{}

		T& operator*() const noexcept (false)
		{
			if (_item == NULL) throw ibrcommon::Exception("object not initialized");
			return *_item;
		}

	private:
		T* _item;
		MutexLock _l;
	};

} // namespace ibrcommon

#endif // SHAREDREFERENCE_H_
