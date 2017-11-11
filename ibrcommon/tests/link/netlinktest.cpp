/*
 * netlinktest.cpp
 *
 *  Created on: 11.10.2012
 *      Author: morgenro
 */

#include "link/netlinktest.h"
#include <ibrcommon/link/LinkManager.h>
#include <ibrcommon/net/vinterface.h>
#include <ibrcommon/net/vaddress.h>
#include <list>

CPPUNIT_TEST_SUITE_REGISTRATION (netlinktest);

void netlinktest :: setUp (void)
{
}

void netlinktest :: tearDown (void)
{
}

void netlinktest :: baseTest (void)
{
	ibrcommon::LinkManager::initialize();

	ibrcommon::vinterface iface("eth0");
	try {
		std::list<ibrcommon::vaddress> ret = ibrcommon::LinkManager::getInstance().getAddressList(iface);
		std::cout << "Addresses:" << std::endl;
		for (std::list<ibrcommon::vaddress>::iterator iter = ret.begin(); iter != ret.end(); ++iter)
		{
			std::cout << " " << (*iter).toString() << std::endl;
		}
	} catch(const ibrcommon::Exception &e) {
		std::cout << "\tInterface enumeration is not allowed on this platform: "
				<< std::endl << e.what() << std::endl;
		throw;
	}
}
void netlinktest::upUpTest (void)
{
	ibrcommon::LinkManager::initialize();
	ibrcommon::LinkManager::initialize();
}
