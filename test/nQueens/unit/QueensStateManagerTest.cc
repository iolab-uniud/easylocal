/*
 *  QueensStateManagerTest.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 25/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <unit/StateManagerTest.hh>
#include "../helpers/QueensStateManager.hh"

class QueensStateManagerTest : public StateManagerTest<unsigned int, std::vector<unsigned>, QueensStateManager>
{
	// This type rename is mandatory for passing the parent class to the CPPUNIT_TEST_SUB_SUITE macro
	// that is not protected against template parameters
	typedef StateManagerTest<unsigned int, std::vector<unsigned>, QueensStateManager> abstractStateManagerTest;
	CPPUNIT_TEST_SUB_SUITE(QueensStateManagerTest, abstractStateManagerTest);
	CPPUNIT_TEST_SUITE_END();
public:
	QueensStateManagerTest() 
	{ 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Input creation raises an exception", __FILE__, __LINE__), this->in = new unsigned int);
		*(this->in) = 5; 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State manager creation raises an exception", __FILE__, __LINE__), this->sm = new QueensStateManager(*this->in));															
	}
	~QueensStateManagerTest() 
	{ 
		delete (this->sm);
	  delete (this->in); 
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(QueensStateManagerTest);
