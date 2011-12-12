#if !defined(_NEIGHBORHOOD_EXPLORER_TEST_HH_)
#define _NEIGHBORHOOD_EXPLORER_TEST_HH_

#include <cppunit/extensions/HelperMacros.h>
#include <unit/TestUtils.hh>

template <typename Input, typename State, typename Move, typename StateManager, typename NeighborhoodExplorer>
class NeighborhoodExplorerTest : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE(NeighborhoodExplorerTest);
	CPPUNIT_TEST(testFirstMove);
	CPPUNIT_TEST(testNextMove);
	CPPUNIT_TEST(testMakeMove);
	CPPUNIT_TEST(testNeighborhoodExploration);
	CPPUNIT_TEST_SUITE_END_ABSTRACT();
protected:
	Input *in; // TODO: add also a set of input objects, all to be verified
	State *st;
	StateManager *sm;
	NeighborhoodExplorer *ne;
	void checkObjects()
	{
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual input should be set in the class constructor before testing", __FILE__, __LINE__), in != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual state manager should be set in the class constructor before testing", __FILE__, __LINE__), sm != NULL);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual neighborhood explorer should be set in the class constructor before testing", __FILE__, __LINE__), ne != NULL);	
	}	
	const unsigned int Trials;
public:
	NeighborhoodExplorerTest() : in(NULL), st(NULL), sm(NULL), ne(NULL), Trials(20) {}
	
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
	
	void testFirstMove() 
	{
		Move mv;
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("First move raises an exception", __FILE__, __LINE__), ne->FirstMove(*st, mv));
		}
	}
	
	void testNextMove()
	{
		Move mv;
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random move raises an exception", __FILE__, __LINE__), ne->RandomMove(*st, mv));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Next move raises an exception", __FILE__, __LINE__), ne->NextMove(*st, mv));
		}	
	}
	
	void testMakeMove()
	{
		Move mv;
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random move raises an exception", __FILE__, __LINE__), ne->RandomMove(*st, mv));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Make move raises an exception", __FILE__, __LINE__), ne->MakeMove(*st, mv));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("State after making the move is not consistent", __FILE__, __LINE__), sm->CheckConsistency(*st));
		}	
	}

	void testNeighborhoodExploration()
	{
		Move mv;
		for (unsigned int i = 0; i < Trials; i++)
		{
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Random state raises an exception", __FILE__, __LINE__), sm->RandomState(*st));
			CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("First move raises an exception", __FILE__, __LINE__), ne->FirstMove(*st, mv));
			bool finished = false;
			do 
			{
				CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Next move raises an exception", __FILE__, __LINE__), finished = ne->NextMove(*st, mv));
			}
			while (!finished);
		}	
	}
};

#endif // _NEIGHBORHOOD_EXPLORER_TEST_HH_
