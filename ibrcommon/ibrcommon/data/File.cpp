/*
 * File.cpp
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

#include "ibrcommon/config.h"
#include "ibrcommon/data/File.h"
#include "ibrcommon/thread/Mutex.h"
#include "ibrcommon/thread/MutexLock.h"
#include <sstream>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <libgen.h>

#ifdef __WIN32__
#include <io.h>
#define FILE_DELIMITER_CHAR '\\'
#define FILE_DELIMITER "\\"
#else
#define FILE_DELIMITER_CHAR '/'
#define FILE_DELIMITER "/"
#endif

namespace ibrcommon
{
	File::File()
	 : _type(DT_UNKNOWN), _path()
	{
	}

	File::File(const std::string &path, const unsigned char t)
	 : _type(t), _path(path)
	{
		resolveAbsolutePath();
		removeSlash();
	}

	File::File(const std::string &path)
	 :_type(DT_UNKNOWN), _path(path)
	{
		resolveAbsolutePath();
		removeSlash();
		update();
	}

	void File::removeSlash()
	{
		if (_path == FILE_DELIMITER) return;

		std::string::iterator iter = _path.end(); --iter;

		if ((*iter) == FILE_DELIMITER_CHAR)
		{
			_path.erase(iter);
		}
	}

	void File::resolveAbsolutePath()
	{
#ifndef __WIN32__
		std::string::iterator iter = _path.begin();

		if ((*iter) != FILE_DELIMITER_CHAR && (*iter) != '.')
		{
			_path = "." + std::string(FILE_DELIMITER) + _path;
		}
#endif
	}

	bool File::exists() const
	{
#ifdef __WIN32__
		DWORD st = GetFileAttributesA(_path.c_str());
		if (st == INVALID_FILE_ATTRIBUTES)
			return false;

		return true;
#else
		struct stat st;
		if( stat(_path.c_str(), &st ) == 0)
			return true;

		return false;
#endif
	}

	void File::update()
	{
#ifdef __WIN32__
		DWORD s = GetFileAttributesA(_path.c_str());
		if (s == INVALID_FILE_ATTRIBUTES) {
			_type = DT_UNKNOWN;
		}
		else if (s & FILE_ATTRIBUTE_DIRECTORY) {
			_type = DT_DIR;
		}
		else {
			_type = DT_REG;
		}
#else
		struct stat s;

		if ( stat(_path.c_str(), &s) == 0 )
		{
			int type = s.st_mode & S_IFMT;

			switch (type)
			{
				case S_IFREG:
					_type = DT_REG;
					break;

				case S_IFLNK:
					_type = DT_LNK;
					break;

				case S_IFDIR:
					_type = DT_DIR;
					break;

				default:
					_type = DT_UNKNOWN;
					break;
			}
		}
#endif
	}

	File::~File()
	{}

	unsigned char File::getType() const
	{
		return _type;
	}

	int File::getFiles(std::list<File> &files) const
	{
		if (!isDirectory()) return -1;

		DIR *dp;
		struct dirent dirp_data;
		struct dirent *dirp;
		if((dp = opendir(_path.c_str())) == NULL) {
			return errno;
		}

#if __WIN32__
		while ((dirp = ::readdir(dp)) != NULL)
#else
		while (::readdir_r(dp, &dirp_data, &dirp) == 0)
#endif
		{
			if (dirp == NULL) break;

			// TODO: check if the name is always limited by a zero
			// std::string name = std::string(dirp->d_name, dirp->d_reclen);

			std::string name = std::string(dirp->d_name);
			std::stringstream ss; ss << getPath() << FILE_DELIMITER_CHAR << name;
#if __WIN32__
			File file(ss.str());
#else
			File file(ss.str(), dirp->d_type);
#endif
			files.push_back(file);
		}
		closedir(dp);

		return 0;
	}

	bool File::isRoot() const
	{
		return _path == FILE_DELIMITER;
	}

	bool File::isSystem() const
	{
		return ((getBasename() == "..") || (getBasename() == "."));
	}

	bool File::isDirectory() const
	{
		if (_type == DT_DIR) return true;
		return false;
	}

	bool File::isValid() const
	{
		return !_path.empty();
	}

	std::string File::getPath() const
	{
		return _path;
	}

	std::string File::getBasename() const
	{
		return std::string(basename((char*)_path.c_str()));
	}

	File File::get(const std::string &filename) const
	{
		std::stringstream ss;
		if (!isRoot()) ss << getPath();
		ss << FILE_DELIMITER_CHAR << filename;

		File file(ss.str());

		return file;
	}

	int File::remove(bool recursive)
	{
		if (isSystem()) return -1;
		if (_type == DT_UNKNOWN) return -1;

		if (isDirectory())
		{
			if (recursive)
			{
				int ret = 0;

				// container for all files
				std::list<File> files;

				// get all files in this directory
				if ((ret = getFiles(files)) < 0)
					return ret;

				for (std::list<File>::iterator iter = files.begin(); iter != files.end(); ++iter)
				{
					ibrcommon::File &file = (*iter);
					if (!file.isSystem())
					{
						if ((ret = file.remove(recursive)) < 0)
							return ret;
					}
				}
			}

			::rmdir(getPath().c_str());
		}
		else
		{
			::remove(getPath().c_str());
		}

		return 0;
	}

	File File::getParent() const
	{
		if (isRoot()) return (*this);

		size_t pos = _path.find_last_of(FILE_DELIMITER_CHAR);
		if (pos == std::string::npos) return (*this);
		if (pos == 0) return File(FILE_DELIMITER);
		return File(_path.substr(0, pos));
	}

	void File::createDirectory(File &path)
	{
		if (!path.exists())
		{
			File parent = path.getParent();
			File::createDirectory(parent);

			// create path
#ifdef __WIN32__
			::mkdir(path.getPath().c_str());
#else
			::mkdir(path.getPath().c_str(), 0700);
#endif

			// update file information
			path.update();
		}
	}

	size_t File::size() const
	{
		struct stat filestatus;
		stat( getPath().c_str(), &filestatus );
		return static_cast<size_t>(filestatus.st_size);
	}

	time_t File::lastaccess() const
	{
		struct stat filestatus;
		stat( getPath().c_str(), &filestatus );
		return filestatus.st_atime;
	}

	time_t File::lastmodify() const
	{
		struct stat filestatus;
		stat( getPath().c_str(), &filestatus );
		return filestatus.st_mtime;
	}

	time_t File::laststatchange() const
	{
		struct stat filestatus;
		stat( getPath().c_str(), &filestatus );
		return filestatus.st_ctime;
	}

	bool File::operator==(const ibrcommon::File &other) const
	{
		return (other._path == _path);
	}

	bool File::operator<(const ibrcommon::File &other) const
	{
		return (_path < other._path);
	}

	TemporaryFile::TemporaryFile(const File &path, const std::string prefix)
	 : File(tmpname(path, prefix))
	{
	}

	TemporaryFile::~TemporaryFile()
	{
	}

	std::string TemporaryFile::tmpname(const File &path, const std::string prefix)
	{
		// check if path is a directory
		if (!path.isDirectory())
		{
			throw ibrcommon::IOException("given path is not a directory.");
		}

		std::string pattern = path.getPath() + FILE_DELIMITER_CHAR + prefix + "XXXXXX";

		std::vector<char> name(pattern.length() + 1);
		::strcpy(&name[0], pattern.c_str());

#ifdef __WIN32__
		// create temporary file - win32 style
		int fd = -1;

		// since _mktemp is not thread-safe and _mktemp_s not available in mingw
		// we have to lock this method globally
		static ibrcommon::Mutex temp_mutex;
		ibrcommon::MutexLock l(temp_mutex);

		// create temporary name
		if (_mktemp(&name[0]) == NULL) {
			if (errno == EINVAL) {
				throw ibrcommon::IOException("_mktemp(): Bad parameter.");
			}
			else if (errno == EEXIST) {
				throw ibrcommon::IOException("_mktemp(): Out of unique filenames.");
			}
		} else {
			// create and open the temporary file
			std::fstream file(&name[0], std::fstream::out | std::fstream::trunc);
			if (!file.good())
				throw ibrcommon::IOException("Could not create a temporary file.");
		}
#else
		int fd = mkstemp(&name[0]);
		if (fd == -1) throw ibrcommon::IOException("Could not create a temporary file.");
		::close(fd);
#endif

		return std::string(name.begin(), name.end());
	}
}
