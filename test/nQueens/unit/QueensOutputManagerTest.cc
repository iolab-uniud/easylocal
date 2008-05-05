/*
 *  QueensOutputManagerTest.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 25/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <unit/OutputManagerTest.hh>
#include "../helpers/QueensOutputManager.hh"

class QueensOutputManagerTest : public OutputManagerTest<unsigned int, ChessBoard, std::vector<unsigned>, QueensStateManager, QueensOutputManager>
{
	// This type rename is mandatory for passing the parent class to the CPPUNIT_TEST_SUB_SUITE macro
	// that is not protected against template parameters
	typedef OutputManagerTest<unsigned int, ChessBoard, std::vector<unsigned>, QueensStateManager, QueensOutputManager> abstractOutputManagerTest;
	CPPUNIT_TEST_SUB_SUITE(QueensOutputManagerTest, abstractOutputManagerTest);
	CPPUNIT_TEST_SUITE_END();
public:
	QueensOutputManagerTest() 
	{ 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Input creation raises an exception", __FILE__, __LINE__), this->in = new unsigned int);
		*(this->in) = 5; 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State manager creation raises an exception", __FILE__, __LINE__), this->sm = new QueensStateManager(*this->in));
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Output manager creation raises an exception", __FILE__, __LINE__), this->om = new QueensOutputManager(*this->in));															
	}
	~QueensOutputManagerTest() 
	{ 
		delete (this->om);
		delete (this->sm);
	  delete (this->in); 
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(QueensOutputManagerTest);
