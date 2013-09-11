/*
 * LinkMonitor.h
 *
 * Copyright (C) 2013 IBR, TU Braunschweig
 *
 * Written-by: David Goltzsche <goltzsch@ibr.cs.tu-bs.de>
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
 *  Created on: Sep 9, 2013
 */

#ifndef LINKREQUESTER_H_
#define LINKREQUESTER_H_


#include <list>
#include "LinkManager.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/thread/Thread.h"

namespace ibrcommon {

	class LinkMonitor : public ibrcommon::JoinableThread {

	public:
		LinkMonitor(LinkManager &lm);
		virtual ~LinkMonitor();

	protected:
		void run() throw ();
		void __cancellation() throw ();
	private:
		bool _running;
		LinkManager &_lmgr;

		std::map<vinterface,std::set<vaddress> > _iface_adr_map;
	};

}
#endif /* LINKREQUESTER_H_ */
