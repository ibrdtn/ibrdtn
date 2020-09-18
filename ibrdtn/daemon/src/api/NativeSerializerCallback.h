/*
 * NativeSerializerCallback.h
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
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

#ifndef NATIVESERIALIZERCALLBACK_H_
#define NATIVESERIALIZERCALLBACK_H_

#include <ibrdtn/data/PrimaryBlock.h>
#include <ibrdtn/data/Block.h>

namespace dtn
{
	namespace api
	{
		class NativeSerializerCallback {
		public:
			virtual ~NativeSerializerCallback() = 0;

			virtual void beginBundle(const dtn::data::PrimaryBlock &block) noexcept = 0;
			virtual void endBundle() noexcept = 0;

			virtual void beginBlock(const dtn::data::Block &block, const size_t payload_length) noexcept = 0;
			virtual void endBlock() noexcept = 0;

			virtual void payload(const char *buf, const size_t len) noexcept = 0;
		};
	}
}
#endif /* NATIVESERIALIZERCALLBACK_H_ */
