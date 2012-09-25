#if !defined(_DELTA_COST_COMPONENT_TEST_HH_)
#define _DELTA_COST_COMPONENT_TEST_HH_

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
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual input should be set in the class constructor before testing", __FILE__, __LINE__), in != nullptr);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual state manager should be set in the class constructor before testing", __FILE__, __LINE__), sm != nullptr);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual neighborhood explorer should be set in the class constructor before testing", __FILE__, __LINE__), ne != nullptr);	
		CPPUNIT_ASSERT_MESSAGE(stringify("Actual delta cost component should be set in the class constructor before testing", __FILE__, __LINE__), dcc != nullptr);	
	}
	const unsigned int Trials;
public:
	DeltaCostComponentTest() : in(nullptr), st(nullptr), sm(nullptr), ne(nullptr), dcc(nullptr), Trials(20) {}
	
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
		State* st1 = nullptr;
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
		State* st1 = nullptr;
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

#endif // _DELTA_COST_COMPONENT_TEST_HH_

