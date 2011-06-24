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

#if !defined(_DELTACOSTCOMPONENTTEST_HH_)
#define _DELTACOSTCOMPONENTTEST_HH_

#include <cppunit/extensions/HelperMacros.h>
#include <unit/TestUtils.hh>

template <typename Input, typename State, typename Move, typename StateManager, typename NeighborhoodExplorer, typename DeltaCostComponent>
class DeltaCostComponentTest : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE(DeltaCostComponentTest);
	CPPUNIT_TEST(testComputeDeltaCost);
	CPPUNIT_TEST(testComputeAllDeltaCosts);
	CPPUNIT_TEST(testComputeThroughNeighborhoodExplorer);
	CPPUNIT_TEST_SUITE_END_ABSTRACT();
protected:
	Input *in; // TODO: add also a set of input objects, all to be verified
	State *st;
	StateManager* sm;
	NeighborhoodExplorer *ne;
	DeltaCostComponent *dcc;
	void checkObjects()
	{
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual input should be set in the class constructor before testing", __FILE__, __LINE__), in != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual state manager should be set in the class constructor before testing", __FILE__, __LINE__), sm != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual neighborhood explorer should be set in the class constructor before testing", __FILE__, __LINE__), ne != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual delta cost component should be set in the class constructor before testing", __FILE__, __LINE__), dcc != NULL);	
	}
	const unsigned int Trials;
public:
	DeltaCostComponentTest() : in(NULL), st(NULL), sm(NULL), ne(NULL), dcc(NULL), Trials(20) {}
	
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
	
	void testComputeDeltaCost() 
	{
		State* st1 = NULL;
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation raises an exception", __FILE__, __LINE__), st1 = new State(*in));	
		bool previous_cc_hard = dcc->cc.is_hard;
		dcc->cc.is_hard = false;
		for (unsigned int i = 0; i < Trials; i++)
		{
			Move mv;			
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random move raises an exception", __FILE__, __LINE__), ne->RandomMove(*st, mv));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Compute delta cost raises an exception", __FILE__, __LINE__), dcc->ComputeDeltaCost(*st, mv));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State copy raises an exception", __FILE__, __LINE__), *st1 = *st);
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Make move raises an exception", __FILE__, __LINE__), ne->MakeMove(*st1, mv));
			CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Cost computed through delta cost component does not correspond to the actual state cost", __FILE__, __LINE__),
																	 dcc->DeltaCost(*st, mv), dcc->cc.Cost(*st1) - dcc->cc.Cost(*st));
		}
		dcc->cc.is_hard = previous_cc_hard;
		delete st1;
	}
		
	void testComputeThroughNeighborhoodExplorer()
	{
		bool previous_cc_hard = dcc->cc.is_hard;
		dcc->cc.is_hard = false;
		for (unsigned int i = 0; i < Trials; i++)
		{
			Move mv;
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Clearing cost components state raises an exception", __FILE__, __LINE__), sm->ClearCostComponents());
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Adding cost component raises an exception", __FILE__, __LINE__), sm->AddCostComponent(dcc->cc));			
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Adding delta cost component raises an exception", __FILE__, __LINE__), ne->AddDeltaCostComponent(*dcc));			
			CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Cost computed by delta cost component is different from the one computed through the Neighborhood Explorer", __FILE__, __LINE__), 
																	 ne->DeltaCostFunction(*st, mv), dcc->DeltaCost(*st, mv));
		}
		dcc->cc.is_hard = previous_cc_hard;
	}
	
	void testComputeAllDeltaCosts() 
	{
		Move mv;
		State* st1 = NULL;
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("First move raises an exception", __FILE__, __LINE__), ne->FirstMove(*st, mv));
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation raises an exception", __FILE__, __LINE__), st1 = new State(*in));	
		bool previous_cc_hard = dcc->cc.is_hard;
		dcc->cc.is_hard = false;
		bool finished = false;
		do 
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Next move raises an exception", __FILE__, __LINE__), finished = ne->NextMove(*st, mv));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State copy raises an exception", __FILE__, __LINE__), *st1 = *st);
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Make move raises an exception", __FILE__, __LINE__), ne->MakeMove(*st1, mv));
			CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Cost computed through delta cost component does not correspond to the actual state cost", __FILE__, __LINE__),
																	 dcc->DeltaCost(*st, mv), dcc->cc.Cost(*st1) - dcc->cc.Cost(*st));
		}
		while (!finished);
		dcc->cc.is_hard = previous_cc_hard;
	}		
};

#endif // _DELTACOSTCOMPONENTTEST_HH_

