/*
 *  OutputManagerTest.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 23/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <cppunit/extensions/HelperMacros.h>
#include <unit/TestUtils.hh>

template <typename Input, typename Output, typename State, typename StateManager, typename OutputManager>
class OutputManagerTest : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE(OutputManagerTest);
	CPPUNIT_TEST(testOuputManager);
	CPPUNIT_TEST_SUITE_END_ABSTRACT();
protected:
	Input *in; // TODO: add also a set of input objects, all to be verified
	State *st;
	Output *out;
	StateManager *sm;
	OutputManager *om;
	void checkObjects()
	{
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual input should be set in the class constructor before testing", __FILE__, __LINE__), in != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual state manager should be set in the class constructor before testing", __FILE__, __LINE__), sm != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual output manager should be set in the class constructor before testing", __FILE__, __LINE__), om != NULL);	
	}
	const unsigned int Trials;
public:
	OutputManagerTest() : in(NULL), st(NULL), out(NULL), sm(NULL), om(NULL), Trials(20) {}
	
	void setUp()
	{
		checkObjects();
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation raises an exception", __FILE__, __LINE__), st = new State(*in));
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Output creation raises an exception", __FILE__, __LINE__), out = new Output(*in));
	}
	
	void tearDown()
	{
		if (out)
			delete out;
		if (st)
			delete st;
	}
	
	void testOuputManager() 
	{
		State *st1;
		CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation raises an exception", __FILE__, __LINE__), st1 = new State(*in));
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State copy raises an exception", __FILE__, __LINE__), *st1 = *st);
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State copied is not consistent", __FILE__, __LINE__), sm->CheckConsistency(*st1));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Output creation through output manager raises an exception", __FILE__, __LINE__), om->OutputState(*st, *out));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State creation through output manager raises an exception", __FILE__, __LINE__), om->InputState(*st, *out));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State created is not consistent", __FILE__, __LINE__), sm->CheckConsistency(*st));
			CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("State copied from and through output manager is not equal to the original one", __FILE__, __LINE__),
																	 *st, *st1);
		}			
		delete st1;
	}
};
