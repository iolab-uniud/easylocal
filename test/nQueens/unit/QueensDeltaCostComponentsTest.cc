/*
 *  QueensCostComponentTest.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 25/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <unit/DeltaCostComponentTest.hh>
#include "../helpers/QueensStateManager.hh"
#include "../helpers/SwapNeighborhoodExplorer.hh"
#include "../helpers/PrimaryDiagonalDeltaCostComponent.hh"
#include "../helpers/SecondaryDiagonalDeltaCostComponent.hh"

template <typename QueensDeltaCostComponent, typename QueensCostComponent>
class QueensDeltaCostComponentTest : public DeltaCostComponentTest<int, std::vector<int>, Swap, 
  QueensStateManager, SwapNeighborhoodExplorer, QueensDeltaCostComponent>
{
	// This type rename is mandatory for passing the parent class to the CPPUNIT_TEST_SUB_SUITE macro
	// that is not protected against template parameters
	typedef DeltaCostComponentTest<int, std::vector<int>, Swap, QueensStateManager, 
		SwapNeighborhoodExplorer, QueensDeltaCostComponent> abstractDeltaCostComponentTest;
	CPPUNIT_TEST_SUB_SUITE(QueensDeltaCostComponentTest, abstractDeltaCostComponentTest);
	CPPUNIT_TEST_SUITE_END();
	QueensCostComponent* cc;
public:
	QueensDeltaCostComponentTest() 
	{ 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Input creation raises an exception", __FILE__, __LINE__), this->in = new int);
		*(this->in) = 5; 
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State manager creation raises an exception", __FILE__, __LINE__), this->sm = new QueensStateManager(*this->in));	
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Neighborhood explorer creation raises an exception", __FILE__, __LINE__), this->ne = new SwapNeighborhoodExplorer(*this->in, *this->sm));
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Cost component creation raises an exception", __FILE__, __LINE__), cc = new QueensCostComponent(*this->in));
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Delta cost component creation raises an exception", __FILE__, __LINE__), 
																		this->dcc = new QueensDeltaCostComponent(*this->in, *cc));	
	}
	~QueensDeltaCostComponentTest() 
	{ 
		delete (this->sm);
		delete (this->cc);
		delete (this->dcc);
	  delete (this->in); 
	}
};

typedef QueensDeltaCostComponentTest<PrimaryDiagonalDeltaCostComponent, PrimaryDiagonalCostComponent> primaryDeltaCostSignature;
CPPUNIT_TEST_SUITE_REGISTRATION(primaryDeltaCostSignature);
typedef QueensDeltaCostComponentTest<SecondaryDiagonalDeltaCostComponent, SecondaryDiagonalCostComponent> secondaryDeltaCostSignature;
CPPUNIT_TEST_SUITE_REGISTRATION(secondaryDeltaCostSignature);
