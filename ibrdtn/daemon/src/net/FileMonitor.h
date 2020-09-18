/*
 * FileMonitor.h
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

#include "Component.h"
#include "core/Node.h"
#include <ibrcommon/data/File.h>
#include <ibrcommon/net/vsocket.h>
#include <ibrcommon/net/socket.h>
#include <map>

#ifndef FILEMONITOR_H_
#define FILEMONITOR_H_

namespace dtn
{
	namespace net
	{
		class inotifysocket : ibrcommon::basesocket
		{
		public:
			inotifysocket();
			virtual ~inotifysocket();

			virtual void up() noexcept (false);
			virtual void down() noexcept (false);

			void watch(const ibrcommon::File &path, int opts) noexcept (false);

			ssize_t read(char *data, size_t len) noexcept (false);

		private:

			typedef std::map<int, ibrcommon::File> watch_map;
			watch_map _watch_map;
		};

		class FileMonitor : public dtn::daemon::IndependentComponent
		{
		public:
			FileMonitor();
			virtual ~FileMonitor();

			void watch(const ibrcommon::File &watch);

			const std::string getName() const;

		protected:
			void componentUp() noexcept;
			void componentRun() noexcept;
			void componentDown() noexcept;

			void __cancellation() noexcept;

		private:
			void scan();
			bool isActive(const ibrcommon::File &path) const;

			void adopt(const ibrcommon::File &path);

			ibrcommon::File _watch;
			ibrcommon::vsocket _socket;

			typedef std::set<ibrcommon::File> watch_set;
			watch_set _watchset;

			bool _running;

			std::map<ibrcommon::File, dtn::core::Node> _active_paths;

		};
	}
} /* namespace ibrcommon */
#endif /* FILEMONITOR_H_ */
