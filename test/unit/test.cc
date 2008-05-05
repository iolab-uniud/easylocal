/*
 *  main.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 22/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include "../nQueens/data/ChessBoard.hh"

class OutputTest : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE(OutputTest);
	CPPUNIT_TEST(testSetSquare);
	CPPUNIT_TEST_SUITE_END();
private:
	ChessBoard *c;
public:
	void setUp()
	{
		c = new ChessBoard(5);
	}
	
	void tearDown()
	{
		delete c;
	}
	
	void testSetSquare() 
	{
		for (unsigned int i = 0; i < 5; i++)
			for (unsigned int j = 0; j < 5; j++)
			{
				c->SetSquare(i, j, 'Q');
				CPPUNIT_ASSERT((*c)(i, j) == 'Q');
				c->SetSquare(i, j, ' ');
				CPPUNIT_ASSERT((*c)(i, j) == ' ');
			}
	}
};

CPPUNIT_TEST_SUITE_REGISTRATION(OutputTest);


int main(int argc, char *argv[]) 
{
	CPPUNIT_NS::TextUi::TestRunner runner;	
	CPPUNIT_NS::TestResult test_result;
	CPPUNIT_NS::TestFactoryRegistry &registry = CPPUNIT_NS::TestFactoryRegistry::getRegistry();
	
	CPPUNIT_NS::TestResultCollector collected_results;
	test_result.addListener(&collected_results);
	CPPUNIT_NS::BriefTestProgressListener progress;
	test_result.addListener(&progress);
	
	runner.addTest(registry.makeTest());  
	runner.run(test_result);
	
	CPPUNIT_NS::CompilerOutputter compiler_outputter(&collected_results, std::cerr);
	compiler_outputter.write();
  
	return collected_results.wasSuccessful() ? 0 : 1;
}
