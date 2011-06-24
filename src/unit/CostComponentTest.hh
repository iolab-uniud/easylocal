// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2011 Andrea Schaerf, Luca Di Gaspero. 
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

#if !defined(_COSTCOMPONENTTEST_HH_)
#define _COSTCOMPONENTTEST_HH_

#include <cppunit/extensions/HelperMacros.h>
#include <unit/TestUtils.hh>

template <typename Input, typename State, typename StateManager, typename CostComponent>
class CostComponentTest : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE(CostComponentTest);
	CPPUNIT_TEST(testComputeCost);
	CPPUNIT_TEST(testComputeThroughStateManager);
	CPPUNIT_TEST_SUITE_END_ABSTRACT();
protected:
	Input *in; // TODO: add also a set of input objects, all to be verified
	State *st;
	StateManager* sm;
	CostComponent *cc;
	void checkObjects()
	{
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual input should be set in the class constructor before testing", __FILE__, __LINE__), in != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual state manager should be set in the class constructor before testing", __FILE__, __LINE__), sm != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual cost component should be set in the class constructor before testing", __FILE__, __LINE__), cc != NULL);	
	}
	const unsigned int Trials;
public:
	CostComponentTest() : in(NULL), st(NULL), sm(NULL), cc(NULL), Trials(20) {}
	
	void setUp()
	{
		checkObjects();
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation raises an exception", __FILE__, __LINE__), st = new State(*in));
	}
	
	void tearDown()
	{
		if (st)
			delete st;
	}
	
	void testComputeCost() 
	{
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Compute cost raises an exception", __FILE__, __LINE__), cc->ComputeCost(*st));
		}
	}
	
	void testComputeThroughStateManager()
	{
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Clearing cost components state raises an exception", __FILE__, __LINE__), sm->ClearCostComponents());
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Adding cost component raises an exception", __FILE__, __LINE__), sm->AddCostComponent(*cc));			
			bool previous_cc_hard = cc->is_hard;
			cc->is_hard = false;
			CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Cost computed by cost component is different from the one computed through the State Manager", __FILE__, __LINE__), 
																	 sm->CostFunction(*st), cc->Cost(*st));
			cc->is_hard = previous_cc_hard;
		}
	}
};

#endif // _COSTCOMPONENTTEST_HH_

