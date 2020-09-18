/*
 * FileMonitor.cpp
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

#include "config.h"
#include "net/FileMonitor.h"
#include "core/BundleCore.h"
#include <ibrcommon/Logger.h>
#include <ibrcommon/data/ConfigFile.h>

#ifdef HAVE_SYS_INOTIFY_H
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace dtn
{
	namespace net
	{
		inotifysocket::inotifysocket()
		{
		}

		inotifysocket::~inotifysocket()
		{
		}

		void inotifysocket::up() noexcept (false)
		{
			if (_state != SOCKET_DOWN)
				throw ibrcommon::socket_exception("socket is already up");

#ifdef HAVE_SYS_INOTIFY_H
			// initialize fd
			_fd = inotify_init();
#endif

			_state = SOCKET_UP;
		}

		void inotifysocket::down() noexcept (false)
		{
			if (_state != SOCKET_UP)
				throw ibrcommon::socket_exception("socket is not up");

#ifdef HAVE_SYS_INOTIFY_H
			for (watch_map::iterator iter = _watch_map.begin(); iter != _watch_map.end(); ++iter)
			{
				const int wd = (*iter).first;
				inotify_rm_watch(this->fd(), wd);
			}
			_watch_map.clear();

			this->close();
#endif

			_state = SOCKET_DOWN;
		}

		void inotifysocket::watch(const ibrcommon::File &path, int opts) noexcept (false)
		{
#ifdef HAVE_SYS_INOTIFY_H
			int wd = inotify_add_watch(this->fd(), path.getPath().c_str(), opts);
			_watch_map[wd] = path;
#endif
		}

		ssize_t inotifysocket::read(char *data, size_t len) noexcept (false)
		{
#ifdef HAVE_SYS_INOTIFY_H
			return ::read(this->fd(), data, len);
#else
			return 0;
#endif
		}

		FileMonitor::FileMonitor()
		 : _running(true)
		{
		}

		FileMonitor::~FileMonitor()
		{
			join();
		}

		void FileMonitor::watch(const ibrcommon::File &watch)
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("FileMonitor", 5) << "watch on: " << watch.getPath() << IBRCOMMON_LOGGER_ENDL;

			if (!watch.isDirectory())
				throw ibrcommon::Exception("can not watch files, please specify a directory");

			ibrcommon::socketset socks = _socket.getAll();
			if (socks.size() == 0) return;
			inotifysocket &sock = dynamic_cast<inotifysocket&>(**socks.begin());

#ifdef HAVE_SYS_INOTIFY_H
			sock.watch(watch, IN_CREATE | IN_DELETE);
#endif
			_watchset.insert(watch);
		}

		void FileMonitor::componentUp() noexcept
		{
			// routine checked for noexcept on 15.02.2013
			try {
				_socket.up();
			} catch (const ibrcommon::socket_exception &ex) {
				IBRCOMMON_LOGGER_TAG("FileMonitor", error) << ex.what() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void FileMonitor::componentRun() noexcept
		{
#ifdef HAVE_SYS_INOTIFY_H
			while (_running)
			{
				ibrcommon::socketset fds;

				// scan for mounts
				scan();

				try {
					char buf[1024];

					// select on all bound sockets
					_socket.select(&fds, NULL, NULL, NULL);

					// receive from all sockets
					for (ibrcommon::socketset::iterator iter = fds.begin(); iter != fds.end(); ++iter)
					{
						inotifysocket &sock = dynamic_cast<inotifysocket&>(**iter);
						sock.read((char*)&buf, 1024);
					}

					ibrcommon::Thread::sleep(2000);
				} catch (const ibrcommon::vsocket_interrupt&) {
					return;
				} catch (const ibrcommon::vsocket_timeout&) {
					// scan again after timeout
				} catch (const ibrcommon::socket_exception&) {
					return;
				}
			}
#endif
		}

		void FileMonitor::componentDown() noexcept
		{
			_socket.down();
		}

		void FileMonitor::__cancellation() noexcept
		{
			_socket.down();
		}

		void FileMonitor::scan()
		{
			IBRCOMMON_LOGGER_DEBUG_TAG("FileMonitor", 5) << "scan for file changes" << IBRCOMMON_LOGGER_ENDL;

			std::set<ibrcommon::File> watch_set;

			for (watch_set::iterator iter = _watchset.begin(); iter != _watchset.end(); ++iter)
			{
				const ibrcommon::File &path = (*iter);
				std::list<ibrcommon::File> files;

				if (path.getFiles(files) == 0)
				{
					for (std::list<ibrcommon::File>::iterator iter = files.begin(); iter != files.end(); ++iter)
					{
						watch_set.insert(*iter);
					}
				}
				else
				{
					IBRCOMMON_LOGGER_TAG("FileMonitor", error) << "scan of " << path.getPath() << " failed" << IBRCOMMON_LOGGER_ENDL;
				}
			}

			// check for new directories
			for (std::set<ibrcommon::File>::iterator iter = watch_set.begin(); iter != watch_set.end(); ++iter)
			{
				const ibrcommon::File &path = (*iter);
				if (path.isDirectory() && !path.isSystem())
				{
					if (!isActive(path)) {
						adopt(path);
					}
				}
			}

			// look for gone directories
			for (std::map<ibrcommon::File, dtn::core::Node>::iterator iter = _active_paths.begin(); iter != _active_paths.end();)
			{
				const ibrcommon::File &path = (*iter).first;
				const dtn::core::Node &node = (*iter).second;

				if (watch_set.find(path) == watch_set.end())
				{
					dtn::core::BundleCore::getInstance().getConnectionManager().remove(node);
					IBRCOMMON_LOGGER_DEBUG_TAG("FileMonitor", 5) << "Node on drive gone: " << node.getEID().getString() << IBRCOMMON_LOGGER_ENDL;
					_active_paths.erase(iter++);
				}
				else
				{
					++iter;
				}
			}
		}

		void FileMonitor::adopt(const ibrcommon::File &path)
		{
			// look for ".dtndrive" file
			const ibrcommon::File drivefile = path.get(".dtndrive");
			if (drivefile.exists())
			{
				ibrcommon::ConfigFile cf(drivefile.getPath());

				try {
					const dtn::data::EID eid( cf.read<std::string>("EID") );

					dtn::core::Node n(eid);
					const std::string uri = "file://" + path.getPath() + "/" + cf.read<std::string>("PATH");

					n.add(dtn::core::Node::URI(dtn::core::Node::NODE_DISCOVERED, dtn::core::Node::CONN_FILE, uri, 0));
					dtn::core::BundleCore::getInstance().getConnectionManager().add(n);

					_active_paths[path] = n;

					IBRCOMMON_LOGGER_DEBUG_TAG("FileMonitor", 5) << "Node on drive found: " << n.getEID().getString() << IBRCOMMON_LOGGER_ENDL;

				} catch (const ibrcommon::ConfigFile::key_not_found&) {};
			}
		}

		bool FileMonitor::isActive(const ibrcommon::File &path) const
		{
			return (_active_paths.find(path) != _active_paths.end());
		}

		const std::string FileMonitor::getName() const
		{
			return "FileMonitor";
		}
	}
} /* namespace ibrcommon */
