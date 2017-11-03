/* $Id: templateengine.py 2241 2006-05-22 07:58:58Z fischer $ */

///
/// @file        BloomFilterTest.cpp
/// @brief       CPPUnit-Tests for class BloomFilter
/// @author      Author Name (email@mail.address)
/// @date        Created at 2010-11-01
/// 
/// @version     $Revision: 2241 $
/// @note        Last modification: $Date: 2006-05-22 09:58:58 +0200 (Mon, 22 May 2006) $
///              by $Author: fischer $
///

/*
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

#include "BloomFilterTest.hh"
#include "ibrcommon/data/BloomFilter.h"
#include <iostream>
#include <vector>
#include <string>
#include <string.h>


CPPUNIT_TEST_SUITE_REGISTRATION(BloomFilterTest);

/*========================== tests below ==========================*/

/*=== BEGIN tests for class 'DefaultHashProvider' ===*/
void BloomFilterTest::testOperatorEqual()
{
	/* test signature (const DefaultHashProvider &provider) const */

	ibrcommon::DefaultHashProvider ProviderA(2);
	ibrcommon::DefaultHashProvider ProviderB(3);
	CPPUNIT_ASSERT_EQUAL(false, ProviderA.operator ==(ProviderB));

}

void BloomFilterTest::testCount()
{
	/* test signature () const */

	ibrcommon::DefaultHashProvider Provider(10);
	CPPUNIT_ASSERT(10 == Provider.count());

}

void BloomFilterTest::testHashClear()
{
	/* test signature () */

	ibrcommon::DefaultHashProvider Provider(2);
	Provider.clear();
	CPPUNIT_ASSERT(0 == Provider.count());

}

void BloomFilterTest::testHash()
{
	/* test signature (const unsigned char* begin, std::size_t remaining_length) const */

	ibrcommon::DefaultHashProvider ProviderA(5);
	ibrcommon::DefaultHashProvider ProviderB(5);
	ibrcommon::DefaultHashProvider ProviderC(5);
	std::list<ibrcommon::bloom_type> hashes1;
	std::list<ibrcommon::bloom_type> hashes2;
	std::list<ibrcommon::bloom_type> hashes3;
	const unsigned char* cad1;
	const unsigned char* cad2;
	cad1 = (unsigned char*)"example";
	cad2 = (unsigned char*)"sample";
	hashes1 = ProviderA.hash(cad1,sizeof(cad1));
	hashes2 = ProviderB.hash(cad1,sizeof(cad1));
	hashes3 = ProviderC.hash(cad2,sizeof(cad2));
	CPPUNIT_ASSERT_EQUAL(true, ProviderA.operator ==(ProviderB));

	std::list<ibrcommon::bloom_type>::iterator iter1 = hashes1.begin();
	std::list<ibrcommon::bloom_type>::iterator iter2 = hashes2.begin();
	std::list<ibrcommon::bloom_type>::iterator iter3 = hashes3.begin();

	while((iter1!=hashes1.end())&&(iter2!=hashes2.end())&&(iter3!=hashes3.end())){

		CPPUNIT_ASSERT((*iter1)==(*iter2));
		CPPUNIT_ASSERT((*iter1)!=(*iter3));
		++iter1;
		++iter2;
		++iter3;
	}

}

/*=== END   tests for class 'DefaultHashProvider' ===*/

/*=== BEGIN tests for class 'BloomFilter' ===*/
void BloomFilterTest::testLoad()
{
	/* test signature (const cell_type* data, size_t len) */

	ibrcommon::BloomFilter Filter(1024,1024,2);
	const ibrcommon::cell_type* cad1;
	const ibrcommon::cell_type* cad2;
	cad1 = (const ibrcommon::cell_type*)"Hello World\0";
	Filter.load(cad1, 12);
	cad2 = Filter.table();
	int i = 0;
	bool dif;
	while (cad1[i] != 0 && cad2[i] != 0)
	{
		i++;
		CPPUNIT_ASSERT_EQUAL(cad1[i], cad2[i]);
	}
}

void BloomFilterTest::testOperatorAssign()
{
	/* test signature (const BloomFilter& filter) */

	ibrcommon::BloomFilter FilterA(1024,1024,2);
	ibrcommon::BloomFilter FilterB(1024,1024,2);

	std::string strin;
	strin="hello";
	FilterA.insert(strin);
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin));
	FilterB.operator=(FilterA);
	CPPUNIT_ASSERT_EQUAL(true, FilterB.contains(strin));

}

void BloomFilterTest::testOperatorNot()
{
	/* test signature () const */

	ibrcommon::BloomFilter Filter1(0,0,2);
	CPPUNIT_ASSERT_EQUAL(true, Filter1.operator !());

}

void BloomFilterTest::testInsert()
{
	ibrcommon::BloomFilter Filter1(1024,1024,2);

//	 test signature (const unsigned char* key_begin, const std::size_t length)

	const unsigned char* cad1;
	cad1 = (unsigned char*)"ejemplo";
	Filter1.insert(cad1,sizeof(cad1));
	CPPUNIT_ASSERT_EQUAL(true, Filter1.contains(cad1,sizeof(cad1)));

//	 test signature (const std::string& key)

	std::string strin;
	strin="hello";
	Filter1.insert(strin);
	CPPUNIT_ASSERT_EQUAL(true, Filter1.contains(strin));

//	 test signature (const char* data, const std::size_t& length)

	const char* names ="hola";
	Filter1.insert(names,sizeof(names));
	CPPUNIT_ASSERT_EQUAL(true, Filter1.contains(names,sizeof(names)));

}

void BloomFilterTest::testContains()
{
	/* test signature (const std::string& key) const */
	/* test signature (const char* data, const std::size_t& length) const */

	ibrcommon::BloomFilter Filter1(1024,1024,2);
	char word[8];
	for(int j=0; j <=127 ; ++j)
	{
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		Filter1.insert(word);
		CPPUNIT_ASSERT_EQUAL(true, Filter1.contains(word));
	}
	Filter1.clear();
	CPPUNIT_ASSERT_EQUAL(false, Filter1.contains(word));

}

void BloomFilterTest::testContainsAll()
{
	ibrcommon::BloomFilter Filter2(1024,1024,2);
	std::vector<char*> v(127);
	std::vector<char*>::iterator it;
	for(int j=0; j<128 ; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v.assign(j, word);
	}
	Filter2.insert(v.begin(),v.end());
	it = Filter2.contains_all(v.begin(), v.end());
	CPPUNIT_ASSERT(v.end()==it);

}

void BloomFilterTest::testContainsNone()
{
	ibrcommon::BloomFilter Filter2(1024,1024,2);
	std::vector<char*> v(127);
	std::vector<char*>::iterator it;
	for(int j=0; j<128 ; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v.assign(j,word);
	}

	it = Filter2.contains_none(v.begin(), v.end());
	CPPUNIT_ASSERT(v.end()==it);

}

void BloomFilterTest::testOperatorAndAndAssign()
{
	/* test signature (const BloomFilter& filter) */

	ibrcommon::BloomFilter FilterA(1024,1024,2);
	ibrcommon::BloomFilter FilterB(1024,1024,2);

	std::string strin1,strin2,strin3,strin4;
	strin1="hello";
	strin2="byebye";
	FilterA.insert(strin1);
	FilterA.insert(strin2);
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin1));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin2));

	strin3="hola";
	strin4="byebye";
	FilterB.insert(strin3);
	FilterB.insert(strin4);
	CPPUNIT_ASSERT_EQUAL(true, FilterB.contains(strin3));
	CPPUNIT_ASSERT_EQUAL(true, FilterB.contains(strin4));

	FilterA.operator &=(FilterB);
	CPPUNIT_ASSERT_EQUAL(false, FilterA.contains(strin1));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin2));
	CPPUNIT_ASSERT_EQUAL(false, FilterA.contains(strin3));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin4));

}

void BloomFilterTest::testOperatorInclusiveOrAndAssign()
{
	/* test signature (const BloomFilter& filter) */
	ibrcommon::BloomFilter FilterA(1024,1024,2);
	ibrcommon::BloomFilter FilterB(1024,1024,2);

	std::string strin1,strin2,strin3,strin4;
	strin1="hello";
	strin2="byebye";
	FilterA.insert(strin1);
	FilterA.insert(strin2);
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin1));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin2));

	strin3="hola";
	strin4="chau";
	FilterB.insert(strin3);
	FilterB.insert(strin4);
	CPPUNIT_ASSERT_EQUAL(true, FilterB.contains(strin3));
	CPPUNIT_ASSERT_EQUAL(true, FilterB.contains(strin4));

	FilterA.operator |=(FilterB);
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin1));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin2));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin3));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin4));

}

void BloomFilterTest::testOperatorXorAndAssign()
{
	/* test signature (const BloomFilter& filter) */

	ibrcommon::BloomFilter FilterA(1024,1024,2);
	ibrcommon::BloomFilter FilterB(1024,1024,2);

	std::string strin1,strin2,strin3,strin4;
	strin1="hello";
	strin2="byebye";
	FilterA.insert(strin1);
	FilterA.insert(strin2);
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin1));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin2));

	strin3="hola";
	strin4="byebye";
	FilterB.insert(strin3);
	FilterB.insert(strin4);
	CPPUNIT_ASSERT_EQUAL(true, FilterB.contains(strin3));
	CPPUNIT_ASSERT_EQUAL(true, FilterB.contains(strin4));

	FilterA.operator ^=(FilterB);
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin1));
	CPPUNIT_ASSERT_EQUAL(false, FilterA.contains(strin2));
	CPPUNIT_ASSERT_EQUAL(true, FilterA.contains(strin3));
	CPPUNIT_ASSERT_EQUAL(false, FilterA.contains(strin4));

}

void BloomFilterTest::testGetAllocation()
{
	/* test signature () const */

	ibrcommon::BloomFilter FilterA(100,100,1);
	std::vector<char*> v1(100);
	float expResult = 0.1164; //m/n=10  k=1
	for(int j=0; j<100 ; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v1.assign(j,word);
	}

	FilterA.insert(v1.begin(),v1.end());
	float Result = FilterA.getAllocation();
//	cout<<"result:"<<Result<<endl;
//	cout<<"expected result:"<<expResult<<endl;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(expResult, Result, 0.01);


	ibrcommon::BloomFilter FilterB(1000,1000,1);
	std::vector<char*> v2(1000);
	expResult = 0.1173; //m/n=10  k=1
	for(int j=0; j<1000 ; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v2.assign(j,word);
	}

	FilterB.insert(v2.begin(),v2.end());
	Result = FilterB.getAllocation();
//	cout<<"result:"<<Result<<endl;
//	cout<<"expected result:"<<expResult<<endl;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(expResult, Result, 0.01);



	ibrcommon::BloomFilter FilterC(1024,1024,2);
	std::vector<char*> v3(1024);
	expResult = 0.0489; //m/n=8  k=2
	for(int j=0; j<1024 ; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v3.assign(j,word);
	}

	FilterC.insert(v3.begin(),v3.end());
	Result = FilterC.getAllocation();
//	cout<<"result:"<<Result<<endl;
//	cout<<"expected result:"<<expResult<<endl;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(expResult, Result, 0.01);



	ibrcommon::BloomFilter FilterD(256,256,4);
	std::vector<char*> v4(150);
	expResult = 0.003; //m/n=15  k=4
	for(int j=0; j<150 ; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v4.assign(j,word);
	}

	FilterD.insert(v4.begin(),v4.end());
	Result = FilterD.getAllocation();
//	cout<<"result:"<<Result<<endl;
//	cout<<"expected result:"<<expResult<<endl;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(expResult, Result, 0.01);



	ibrcommon::BloomFilter FilterF(16,16,2);
	std::vector<char*> v5(50);
	expResult = 0.2954; //m/n=20  k=2
	for(int j=0; j<51; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v5.assign(j,word);
	}

	FilterF.insert(v5.begin(),v5.end());
	Result = FilterF.getAllocation();
//	cout<<"result:"<<Result<<endl;
//	cout<<"expected result:"<<expResult<<endl;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(expResult, Result, 0.01);



	ibrcommon::BloomFilter FilterG(8,8,5);
	std::vector<char*> v6(10);
	expResult = 0.0480; //m/n=7 k=5
	for(int j=0; j<11; ++j)
	{
		char word[8];
		for(int i=0; i <= 7; ++i)
		{
			word[i] = 33 + rand() % (126 - 23);
		}
		v6.assign(j,word);
	}

	FilterG.insert(v6.begin(),v6.end());
	Result = FilterG.getAllocation();
//	cout<<"result:"<<Result<<endl;
//	cout<<"expected result:"<<expResult<<endl;
	CPPUNIT_ASSERT_DOUBLES_EQUAL(expResult, Result, 0.01);
	/*
	for(int l=10; l<21; ++l)
	{
		ibrcommon::BloomFilter Filter(100*l,1);
		std::vector<char*> v(100);
		std::vector<char*>::iterator it;
		for(int j=0; j<99 ; ++j)
		{
			char word[8];
			for(int i=0; i <= 7; ++i)
			{
				word[i] = 33 + rand() % (126 - 23);
			}
			v.push_back(word);
		}

		Filter.insert(v.begin(),v.end());

		long r=0;
		double tests = 100000;
		for(int k=0; k<tests; ++k){
			char word[8];
			for(int i=0; i <= 7; ++i)
			{
				word[i] = 33 + rand() % (126 - 23);
			}
			if(Filter.contains(word,sizeof(word))){
				std::vector<char*>::iterator it;
				it = find(v.begin(),v.end(),word);
				if(it!=v.end()){
					r++;
				}
			}
		}

		double ratio = r/tests;
		//cout<<"ratio"<<ratio<<endl;
		float maths = Filter.getAllocation();
		cout<<"maths"<<maths<<endl;
	//	CPPUNIT_ASSERT_DOUBLES_EQUAL(ratio,maths,0.01);
	}
*/
}

void BloomFilterTest::testGrow()
{
	ibrcommon::BloomFilter f(1,128,2);
	CPPUNIT_ASSERT_EQUAL((size_t)1, f.size());

	f.grow(1);

	CPPUNIT_ASSERT_EQUAL((size_t)32, f.size());

	f.grow(10);

	CPPUNIT_ASSERT_EQUAL((size_t)128, f.size());
}

void BloomFilterTest::testMemory()
{
	ibrcommon::BloomFilter Filter1(1,1,1);
	Filter1.insert("test");
	CPPUNIT_ASSERT(Filter1.contains("test"));
	Filter1.clear();
	CPPUNIT_ASSERT(!Filter1.contains("test"));

}
/*=== END   tests for class 'BloomFilter' ===*/

void BloomFilterTest::setUp()
{
}

void BloomFilterTest::tearDown()
{
}

