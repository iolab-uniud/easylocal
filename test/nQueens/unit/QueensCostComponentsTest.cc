/*
 *  QueensCostComponentTest.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 25/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <unit/CostComponentTest.hh>
#include "../helpers/QueensStateManager.hh"
#include "../helpers/PrimaryDiagonalCostComponent.hh"
#include "../helpers/SecondaryDiagonalCostComponent.hh"

template <typename QueensCostComponent>
class QueensCostComponentTest : public CostComponentTest<int, std::vector<int>, QueensStateManager, QueensCostComponent>
{
	// This type rename is mandatory for passing the parent class to the CPPUNIT_TEST_SUB_SUITE macro
	// that is not protected against template parameters
	typedef CostComponentTest<int, std::vector<int>, QueensStateManager, QueensCostComponent> abstractCostComponentTest;
	CPPUNIT_TEST_SUB_SUITE(QueensCostComponentTest, abstractCostComponentTest);
	CPPUNIT_TEST_SUITE_END();
public:
	QueensCostComponentTest() 
	{ 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Input creation raises an exception", __FILE__, __LINE__), this->in = new int);
		*(this->in) = 5; 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State manager creation raises an exception", __FILE__, __LINE__), this->sm = new QueensStateManager(*this->in));	
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Cost component creation raises an exception", __FILE__, __LINE__), this->cc = new QueensCostComponent(*this->in));	
	}
	~QueensCostComponentTest() 
	{ 
		delete (this->sm);
		delete (this->cc);
	  delete (this->in); 
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(QueensCostComponentTest<PrimaryDiagonalCostComponent>);
CPPUNIT_TEST_SUITE_REGISTRATION(QueensCostComponentTest<SecondaryDiagonalCostComponent>);
