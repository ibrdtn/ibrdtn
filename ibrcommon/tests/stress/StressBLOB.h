/*
 * StressBLOB.h
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

#include <StressModule.h>
#include <ibrcommon/data/BLOB.h>
#include <ibrcommon/thread/Thread.h>
#include <list>

#ifndef STRESSBLOB_H_
#define STRESSBLOB_H_

class StressBLOB : public StressModule
{
public:
	StressBLOB();
	virtual ~StressBLOB();

	void stage1();
	void stage2();
	void stage3();

	bool check();

private:
	class BLOBWorker : public ibrcommon::JoinableThread
	{
	public:
		BLOBWorker(const size_t count, const size_t size);
		virtual ~BLOBWorker();

		void run() noexcept;
		bool check();
		void __cancellation() noexcept;

	private:
		size_t _count;
		size_t _size;
		std::string teststr;

		std::list<ibrcommon::BLOB::Reference> _refs;
	};

	std::list<BLOBWorker*> _worker;
};

#endif /* STRESSBLOB_H_ */
