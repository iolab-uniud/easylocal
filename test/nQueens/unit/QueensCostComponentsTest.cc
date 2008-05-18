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
