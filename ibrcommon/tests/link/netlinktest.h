/*
 * netlinktest.h
 *
 *  Created on: 11.10.2012
 *      Author: morgenro
 */

#ifndef NETLINKTEST_H_
#define NETLINKTEST_H_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>


class netlinktest : public CPPUNIT_NS :: TestFixture
{
	CPPUNIT_TEST_SUITE (netlinktest);
	CPPUNIT_TEST (baseTest);
	CPPUNIT_TEST (upUpTest);
	CPPUNIT_TEST_SUITE_END ();

	public:
		void setUp (void);
		void tearDown (void);

	protected:
		void baseTest (void);
		void upUpTest (void);
};

#endif /* NETLINKTEST_H_ */
