/*
 * StressBLOB.cpp
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

#include "StressBLOB.h"
#include <ibrcommon/thread/MutexLock.h>

StressBLOB::StressBLOB()
{
	for (unsigned int i = 0; i < 100; ++i)
	{
		_worker.push_back(new BLOBWorker(100, 10000));
	}
}

StressBLOB::~StressBLOB()
{
	for (std::list<BLOBWorker*>::const_iterator iter = _worker.begin(); iter != _worker.end(); ++iter)
	{
		delete (*iter);
	}
}

void StressBLOB::stage1()
{
	for (std::list<BLOBWorker*>::const_iterator iter = _worker.begin(); iter != _worker.end(); ++iter)
	{
		std::cout << "+";
		(*iter)->start();
	}
}

void StressBLOB::stage2()
{
}

void StressBLOB::stage3()
{
	for (std::list<BLOBWorker*>::const_iterator iter = _worker.begin(); iter != _worker.end(); ++iter)
	{
		(*iter)->join();
	}

	std::cout << "#" << std::endl;
}

bool StressBLOB::check()
{
	bool err = false;
	for (std::list<BLOBWorker*>::const_iterator iter = _worker.begin(); iter != _worker.end(); ++iter)
	{
		if (!(*iter)->check()) err = true;
		std::cout << "$";
	}

	std::cout << std::endl;

	return !err;
}

StressBLOB::BLOBWorker::BLOBWorker(const size_t count, const size_t size)
 : _count(count), _size(size), teststr("0123456789")
{
}

StressBLOB::BLOBWorker::~BLOBWorker()
{}

bool StressBLOB::BLOBWorker::check()
{
	for (std::list<ibrcommon::BLOB::Reference>::iterator iter = _refs.begin(); iter != _refs.end(); ++iter)
	{
		ibrcommon::BLOB::Reference &ref = (*iter);
		ibrcommon::BLOB::iostream stream = ref.iostream();

		for (unsigned int i = 0; i < _count; ++i)
		{
			for (unsigned int k = 0; k < teststr.length(); ++k)
			{
				char v = (*stream).get();
				char e = teststr.c_str()[k];
				if (e != v)
				{
					std::cerr << "ERROR: wrong letter found in BLOB stream. " << " expected: " << e << ", found: " << v << std::endl;
					return false;
				}
			}
		}
	}

	return true;
}

void StressBLOB::BLOBWorker::run() noexcept
{
	for (unsigned int i = 0; i < _count; ++i)
	{
		ibrcommon::BLOB::Reference ref = ibrcommon::BLOB::create();
		_refs.push_back(ref);

		// write some test data into the blob
		{
			ibrcommon::BLOB::iostream stream = ref.iostream();

			for (unsigned int j = 0; j < _size; ++j)
			{
				(*stream) << teststr;
			}

			(*stream).flush();
		}

		std::cout << "." << std::flush;
	}
}

void StressBLOB::BLOBWorker::__cancellation() noexcept
{
}
