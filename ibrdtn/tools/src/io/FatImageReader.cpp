/*
 * FatImageReader.cpp
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
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
 *  Created on: Mar 14, 2014
 */

#include "FatImageReader.h"
#include <ibrcommon/Logger.h>
#include <sstream>

namespace io
{
	const std::string FatImageReader::TAG = "FatImageReader";

	FatImageReader::FatImageException::FatImageException(int errcode, const std::string &operation, const ibrcommon::File &file)
	 : ibrcommon::IOException(create_message(errcode, operation, file)), _errorcode(errcode)
	{
	}

	FatImageReader::FatImageException::~FatImageException() noexcept
	{
	}

	std::string FatImageReader::FatImageException::create_message(int errcode, const std::string &operation, const ibrcommon::File &file)
	{
		std::stringstream ss;
		ss << operation << " failed with " << errcode << " for " << file.getPath();
		return ss.str();
	}

	int FatImageReader::FatImageException::getErrorCode() const
	{
		return _errorcode;
	}

	FatImageReader::FatImageReader(const ibrcommon::File &filename)
	 : _filename(filename)
	{
	}

	FatImageReader::~FatImageReader()
	{
	}

	bool FatImageReader::isDirectory(const FATFile &file) const noexcept
	{
		try {
			if (file.isRoot()) return true;

			dirent_t d;
			update(file, d);
			return d.dir_attr & DIR_ATTR_DIRECTORY;
		} catch (const ibrcommon::IOException &e) {
			return false;
		}
	}

	size_t FatImageReader::size(const FATFile &file) const noexcept
	{
		try {
			if (file.isRoot()) return 0;

			dirent_t d;
			update(file, d);
			return d.dir_file_size;
		} catch (const ibrcommon::IOException &e) {
			return 0;
		}
	}

	time_t FatImageReader::lastaccess(const FATFile &file) const noexcept
	{
		try {
			if (file.isRoot()) return 0;

			dirent_t d;
			update(file, d);

			struct tm tm;
			tm.tm_year = d.crttime.year - 1900;
			tm.tm_mon  = d.crttime.month - 1;
			tm.tm_mday = d.crttime.day;
			tm.tm_hour = d.crttime.hour;
			tm.tm_min  = d.crttime.min;
			tm.tm_sec  = d.crttime.sec / 2; //somehow, libtffs writes seconds from 0-119;
			return mktime(&tm);
		} catch (const ibrcommon::IOException &e) {
			return 0;
		}
	}

	bool FatImageReader::exists(const FATFile &file) const noexcept
	{
		try {
			dirent_t d;
			update(file, d);
			return true;
		} catch (const ibrcommon::IOException &e) {
			return false;
		}
	}

	void FatImageReader::update(const FATFile &path, dirent_t &d) const noexcept (false)
	{
		int ret = 0;
		tdir_handle_t hdir;
		tffs_handle_t htffs;
		bool found = false;

		// root does not have a dirent entry
		if (path.isRoot()) return;

		// open fat image
		byte* image_path = const_cast<char *>(_filename.getPath().c_str());
		if ((ret = TFFS_mount(image_path, &htffs)) != TFFS_OK) {
			throw FatImageException(ret, "TFFS_mount", _filename);
		}

		// get parent directory
		FATFile parent = path.getParent();

		// open directory
		const std::string dir_path = parent.getPath() + (parent.isRoot() ? "" : "/");
		if ((ret = TFFS_opendir(htffs, const_cast<char *>(dir_path.c_str()), &hdir)) != TFFS_OK) {
			IBRCOMMON_LOGGER_TAG(TAG, error) << "TFFS_opendir failed" << IBRCOMMON_LOGGER_ENDL;
		}
		else
		{
			// list files, searching the selected one
			while( 1 )
			{
				ret = TFFS_readdir(hdir, &d);
				if (ret == TFFS_OK) {
					if (d.d_name == path.getBasename()) {
						found = true;
						break;
					}
				} else {
					// "ret" might be "ERR_TFFS_LAST_DIRENTRY"
					break;
				}
			}

			// close directory
			if ((ret = TFFS_closedir(hdir)) != TFFS_OK) {
				IBRCOMMON_LOGGER_TAG(TAG, error) << "TFFS_closedir failed" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		// close image file
		if ((ret = TFFS_umount(htffs)) != TFFS_OK) {
			throw FatImageException(ret, "TFFS_umount", _filename);
		}

		if (!found) {
			throw ibrcommon::IOException("File not found");
		}
	}

	void FatImageReader::list(filelist &files) const noexcept (false)
	{
		FATFile root(*this, "/");
		list(root, files);
	}

	void FatImageReader::list(const FATFile &directory, filelist &files) const noexcept (false)
	{
		int ret = 0;
		tdir_handle_t hdir;
		dirent_t dirent;
		tffs_handle_t htffs;

		// open fat image
		byte* image_path = const_cast<char *>(_filename.getPath().c_str());
		if ((ret = TFFS_mount(image_path, &htffs)) != TFFS_OK) {
			throw FatImageException(ret, "TFFS_mount", _filename);
		}

		// open directory
		const std::string dir_path = directory.getPath() + (directory.isRoot() ? "" : "/");
		if ((ret = TFFS_opendir(htffs, const_cast<char *>(dir_path.c_str()), &hdir)) != TFFS_OK) {
			IBRCOMMON_LOGGER_TAG(TAG, error) << "TFFS_opendir failed" << IBRCOMMON_LOGGER_ENDL;
		}
		else
		{
			while( 1 )
			{
				if ((ret = TFFS_readdir(hdir, &dirent)) == TFFS_OK)
				{
					files.push_back(directory.get(dirent.d_name));
				}
				else if (ret == ERR_TFFS_LAST_DIRENTRY) { // end of directory
					break;
				}
				else {
					break;
				}
			}

			// close directory
			if ((ret = TFFS_closedir(hdir)) != TFFS_OK) {
				IBRCOMMON_LOGGER_TAG(TAG, error) << "TFFS_closedir failed" << IBRCOMMON_LOGGER_ENDL;
			}
		}

		// close image file
		if ((ret = TFFS_umount(htffs)) != TFFS_OK) {
			throw FatImageException(ret, "TFFS_umount", _filename);
		}
	}

	FatImageReader::FileHandle FatImageReader::open(const FATFile &file) const
	{
		return FatImageReader::FileHandle(_filename, file.getPath());
	}

	FatImageReader::FileHandle::FileHandle(const ibrcommon::File &image, const std::string &p)
	 : _open(false)
	{
		int ret = 0;

		// mount tffs
		byte* path = const_cast<char *>(image.getPath().c_str());
		if ((ret = TFFS_mount(path, &_htffs)) != TFFS_OK)
		{
			throw FatImageException(ret, "TFFS_mount", image);
		}

		try {
			// open file
			byte* file = const_cast<char*>(p.c_str());
			if ((ret = TFFS_fopen(_htffs, file, "r", &_hfile)) != TFFS_OK)
			{
				throw FatImageException(ret, "TFFS_fopen", p);
			}

			_open = true;
		} catch (const FatImageException&) {
			// unmount
			TFFS_umount(_htffs);
			throw;
		}
	}

	FatImageReader::FileHandle::~FileHandle()
	{
		close();
	}

	size_t FatImageReader::FileHandle::read(unsigned char *buff, size_t buf_size)
	{
		ssize_t ret = 0;

		// read file
		if (( ret = TFFS_fread(_hfile, buf_size, (unsigned char*) buff)) < 0)
		{
				if( ret == ERR_TFFS_FILE_EOF) return 0;
				else
				{
					std::stringstream ss;
					ss << "TFFS_fread " << ret;
					throw ibrcommon::IOException(ss.str());
				}
		}
		else
		{
			return (ret >= 0) ? (size_t)ret : 0;
		}
	}

	void FatImageReader::FileHandle::close()
	{
		if (!_open) return;

		if (TFFS_fclose(_hfile) != TFFS_OK) {
			IBRCOMMON_LOGGER_TAG("FatFileHandle", error) << "TFFS_fclose failed" << IBRCOMMON_LOGGER_ENDL;
		}

		// close image file
		if (TFFS_umount(_htffs) != TFFS_OK) {
			IBRCOMMON_LOGGER_TAG("FatFileHandle", error) << "TFFS_umount failed" << IBRCOMMON_LOGGER_ENDL;
		}
	}
} /* namespace io */
