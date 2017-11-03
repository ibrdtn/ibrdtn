/*
 * BLOB.cpp
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

#include "ibrcommon/data/BLOB.h"
#include "ibrcommon/thread/MutexLock.h"
#include "ibrcommon/Exceptions.h"
#include "ibrcommon/Logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cstring>
#include <cerrno>
#include <vector>

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

namespace ibrcommon
{
	// maximum of concurrent opened files
	ibrcommon::Semaphore BLOB::_filelimit(10);

	// default BLOB provider - memory based; auto deletion enabled
	ibrcommon::BLOB::ProviderRef BLOB::provider(new ibrcommon::MemoryBLOBProvider(), true);

	BLOB::BLOB(const std::streamsize intitial_size)
	 : _const_size(intitial_size)
	{ }

	BLOB::~BLOB()
	{
	}

	std::streamsize BLOB::size() const
	{
		return _const_size;
	}

	void BLOB::update()
	{
		_const_size = __get_size();
	}

	std::ostream& BLOB::copy(std::ostream &output, std::istream &input, const std::streamsize size, const size_t buffer_size)
	{
		// read payload
		std::vector<char> buffer(buffer_size);

		size_t remain = size;

		while (remain > 0)
		{
			// something bad happened, abort!
			if (input.bad())
			{
				std::stringstream errmsg; errmsg << "input stream went bad [" << std::strerror(errno) << "]; " << (size-remain) << " of " << size << " bytes copied";
				throw ibrcommon::IOException(errmsg.str());
			}

			// reached EOF too early
			if (input.eof())
			{
				std::stringstream errmsg; errmsg << "input stream reached EOF [" << std::strerror(errno) << "]; " << (size-remain) << " of " << size << " bytes copied";
				throw ibrcommon::IOException(errmsg.str());
			}

			// check if the destination stream is ok
			if (output.bad())
			{
				std::stringstream errmsg; errmsg << "output stream went bad [" << std::strerror(errno) << "]; " << (size-remain) << " of " << size << " bytes copied";
				throw ibrcommon::IOException(errmsg.str());
			}

			// retry if the read failed
			if (input.fail())
			{
				IBRCOMMON_LOGGER_TAG("BLOB::copy", warning) << "input stream failed [" << std::strerror(errno) << "]; " << (size-remain) << " of " << size << " bytes copied" << IBRCOMMON_LOGGER_ENDL;
				input.clear();
			}

			// if the last write failed, then retry
			if (output.fail())
			{
				IBRCOMMON_LOGGER_TAG("BLOB::copy", warning) << "output stream failed [" << std::strerror(errno) << "]; " << (size-remain) << " of " << size << " bytes copied" << IBRCOMMON_LOGGER_ENDL;
				output.clear();
			}
			else
			{
				// read the full buffer size of less?
				if (remain > buffer_size)
				{
					input.read(&buffer[0], buffer_size);
				}
				else
				{
					input.read(&buffer[0], remain);
				}

				// retry if the read failed
				if (!input.eof() && input.fail()) continue;
			}

			// write the bytes to the BLOB
			output.write(&buffer[0], input.gcount());

			// shrink the remaining bytes by the red bytes
			remain -= input.gcount();
		}

		return output;
	}

	ibrcommon::BLOB::Reference BLOB::create()
	{
		return ibrcommon::BLOB::provider.create();
	}

	ibrcommon::BLOB::Reference BLOB::open(const ibrcommon::File &f)
	{
		return ibrcommon::BLOB::Reference(new ibrcommon::FileBLOB(f));
	}

	void BLOB::changeProvider(BLOB::Provider *p, bool auto_delete)
	{
		ibrcommon::BLOB::provider.change(p, auto_delete);
	}

	BLOB::Provider::~Provider()
	{ }

	BLOB::ProviderRef::ProviderRef(Provider *provider, bool auto_delete)
	 : _provider(provider), _auto_delete(auto_delete)
	{
	}

	BLOB::ProviderRef::~ProviderRef()
	{
		if (_auto_delete)
		{
			delete _provider;
		}
	}

	void BLOB::ProviderRef::change(BLOB::Provider *p, bool auto_delete)
	{
		if (_auto_delete)
		{
			delete _provider;
		}

		_provider = p;
		_auto_delete = auto_delete;
	}

	BLOB::Reference BLOB::ProviderRef::create()
	{
		return _provider->create();
	}

	MemoryBLOBProvider::MemoryBLOBProvider()
	{
	}

	MemoryBLOBProvider::~MemoryBLOBProvider()
	{
	}

	ibrcommon::BLOB::Reference MemoryBLOBProvider::create()
	{
		return StringBLOB::create();
	}

	FileBLOBProvider::FileBLOBProvider(const File &path)
	 : _tmppath(path)
	{
	}

	FileBLOBProvider::~FileBLOBProvider()
	{
	}

	ibrcommon::BLOB::Reference FileBLOBProvider::create()
	{
		return ibrcommon::BLOB::Reference(new TmpFileBLOB(_tmppath));
	}

	BLOB::Reference::Reference(const Reference &ref)
	 : _blob(ref._blob)
	{
	}

	BLOB::Reference::~Reference()
	{
	}

	std::streamsize BLOB::Reference::size() const
	{
		return _blob->size();
	}

	BLOB::iostream BLOB::Reference::iostream()
	{
		return BLOB::iostream(*_blob);
	}

	BLOB::Reference::Reference(BLOB *blob)
	 : _blob(blob)
	{
	}

	const BLOB& BLOB::Reference::operator*() const
	{
		return *_blob;
	}

	BLOB::Reference MemoryBLOBProvider::StringBLOB::create()
	{
		BLOB::Reference ref(new MemoryBLOBProvider::StringBLOB());
		return ref;
	}

	void MemoryBLOBProvider::StringBLOB::clear()
	{
		_stringstream.str("");
	}

	MemoryBLOBProvider::StringBLOB::StringBLOB()
	 : BLOB(), _stringstream()
	{

	}

	MemoryBLOBProvider::StringBLOB::~StringBLOB()
	{
	}

	void MemoryBLOBProvider::StringBLOB::open()
	{
		// set pointer to the beginning of the stream and remove any error flags
		_stringstream.clear();
		_stringstream.seekp(0);
		_stringstream.seekg(0);
	}

	void MemoryBLOBProvider::StringBLOB::close()
	{
	}

	std::streamsize MemoryBLOBProvider::StringBLOB::__get_size()
	{
		// store current position
		std::streamoff pos = _stringstream.tellg();

		// clear all marker (EOF, fail, etc.)
		_stringstream.clear();

		_stringstream.seekg(0, std::ios_base::end);
		std::streamoff size = _stringstream.tellg();
		_stringstream.seekg(pos);

		return static_cast<std::streamsize>(size);
	}

	void FileBLOB::clear()
	{
		throw ibrcommon::IOException("clear is not possible on a read only file");
	}

	FileBLOB::FileBLOB(const File &f)
	 : ibrcommon::BLOB(f.size()), _filestream(), _file(f)
	{
		if (!f.exists())
		{
			throw ibrcommon::FileNotExistsException(f);
		}
	}

	FileBLOB::~FileBLOB()
	{
	}

	void FileBLOB::open()
	{
		BLOB::_filelimit.wait();

		// open the file
		_filestream.open(_file.getPath().c_str(), std::ios::in|std::ios::binary);

		if (!_filestream.is_open())
		{
			// Release semaphore on failed file open
			ibrcommon::BLOB::_filelimit.post();

			throw ibrcommon::CanNotOpenFileException(_file);
		}
	}

	void FileBLOB::close()
	{
		// close the file
		_filestream.close();

		BLOB::_filelimit.post();
	}

	std::streamsize FileBLOB::__get_size()
	{
		return _file.size();
	}

	void FileBLOBProvider::TmpFileBLOB::clear()
	{
		// close the file
		_filestream.close();

		// open temporary file
		_filestream.open(_tmpfile.getPath().c_str(), std::ios::in | std::ios::out | std::ios::trunc | std::ios::binary );

		if (!_filestream.is_open())
		{
			IBRCOMMON_LOGGER_TAG("TmpFileBLOB::clear", error) << "can not open temporary file " << _tmpfile.getPath() << IBRCOMMON_LOGGER_ENDL;
			throw ibrcommon::CanNotOpenFileException(_tmpfile);
		}
	}

	FileBLOBProvider::TmpFileBLOB::TmpFileBLOB(const File &tmppath)
	 : BLOB(), _filestream(), _fd(0), _tmpfile(tmppath, "blob")
	{
	}

	FileBLOBProvider::TmpFileBLOB::~TmpFileBLOB()
	{
		// delete the file if the last reference is destroyed
		_tmpfile.remove();
	}

	void FileBLOBProvider::TmpFileBLOB::open()
	{
		BLOB::_filelimit.wait();

		// open temporary file
		_filestream.open(_tmpfile.getPath().c_str(), std::ios::in | std::ios::out | std::ios::binary );

		if (!_filestream.is_open())
		{
			// Release semaphore on failed file open
			ibrcommon::BLOB::_filelimit.post();

			IBRCOMMON_LOGGER_TAG("TmpFileBLOB::open", error) << "can not open temporary file " << _tmpfile.getPath() << IBRCOMMON_LOGGER_ENDL;
			throw ibrcommon::CanNotOpenFileException(_tmpfile);
		}
	}

	void FileBLOBProvider::TmpFileBLOB::close()
	{
		// flush the filestream
		_filestream.flush();

		// close the file
		_filestream.close();

		BLOB::_filelimit.post();
	}

	std::streamsize FileBLOBProvider::TmpFileBLOB::__get_size()
	{
		return _tmpfile.size();
	}
}
