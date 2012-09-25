#if !defined(_STATE_MANAGER_TEST_HH_)
#define _STATE_MANAGER_TEST_HH_

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
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual input should be set in the class constructor before testing", __FILE__, __LINE__), in != nullptr);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual state manager should be set in the class constructor before testing", __FILE__, __LINE__), sm != nullptr);	
	}
	const unsigned int Trials;
public:
	StateManagerTest() : in(nullptr), st(nullptr), sm(nullptr), Trials(20) {}
	
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
		State *st1 = nullptr, *st2 = nullptr;
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

#endif // _STATE_MANAGER_TEST_HH_

