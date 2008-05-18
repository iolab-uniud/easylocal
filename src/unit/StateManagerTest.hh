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

#if !defined(_STATEMANAGERTEST_HH_)
#define _STATEMANAGERTEST_HH_

#include <cppunit/extensions/HelperMacros.h>
#include <unit/TestUtils.hh>

template <typename Input, typename State, typename StateManager>
class StateManagerTest : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE(StateManagerTest);
	CPPUNIT_TEST(testRandomState);
	CPPUNIT_TEST_SUITE_END_ABSTRACT();
protected:
	Input *in; // TODO: add also a set of input objects, all to be verified
	State *st;
	StateManager *sm;
	void checkObjects()
	{
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual input should be set in the class constructor before testing", __FILE__, __LINE__), in != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual state manager should be set in the class constructor before testing", __FILE__, __LINE__), sm != NULL);	
	}
	const unsigned int Trials;
public:
	StateManagerTest() : in(NULL), st(NULL), sm(NULL), Trials(20) {}
	
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
	
	void testRandomState() 
	{
		State *st1 = NULL, *st2 = NULL;
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation raises an exception", __FILE__, __LINE__), st1 = new State(*in));
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State copy raises an exception", __FILE__, __LINE__), *st1 = *st);
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State copied is not consisntent", __FILE__, __LINE__), sm->CheckConsistency(*st1));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation through copy constructor raises an exception", __FILE__, __LINE__), st2 = new State(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State copied is not consisntent", __FILE__, __LINE__), sm->CheckConsistency(*st2));
			delete st2;
		}			
		delete st1;
	}
};

#endif // _STATEMANAGERTEST_HH_

