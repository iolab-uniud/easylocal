/*
 *  NeighborhoodExplorerTest.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 25/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <unit/NeighborhoodExplorerTest.hh>
#include "../helpers/SwapNeighborhoodExplorer.hh"

class SwapNeighborhoodExplorerTest : public NeighborhoodExplorerTest<int, std::vector<int>, Swap, QueensStateManager, SwapNeighborhoodExplorer>
{
	// This type rename is mandatory for passing the parent class to the CPPUNIT_TEST_SUB_SUITE macro
	// that is not protected against template parameters
	typedef NeighborhoodExplorerTest<int, std::vector<int>, Swap, QueensStateManager, SwapNeighborhoodExplorer> abstractNeighborhoodExplorerTest;
	CPPUNIT_TEST_SUB_SUITE(SwapNeighborhoodExplorerTest, abstractNeighborhoodExplorerTest);
	CPPUNIT_TEST_SUITE_END();
public:
	SwapNeighborhoodExplorerTest() 
	{ 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Input creation raises an exception", __FILE__, __LINE__), this->in = new int);
		*(this->in) = 5; 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State manager creation raises an exception", __FILE__, __LINE__), this->sm = new QueensStateManager(*this->in));
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Neighborhood explorer creation raises an exception", __FILE__, __LINE__), this->ne = new SwapNeighborhoodExplorer(*this->in, *this->sm));
	}
	~SwapNeighborhoodExplorerTest() 
	{ 
		delete (this->ne);
		delete (this->sm);
		delete (this->in); 
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(SwapNeighborhoodExplorerTest);
