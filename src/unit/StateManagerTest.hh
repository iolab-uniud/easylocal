/*
 *  StateManagerTest.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 23/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

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
		State *st1, *st2;
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
