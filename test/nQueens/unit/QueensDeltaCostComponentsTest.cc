// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

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
