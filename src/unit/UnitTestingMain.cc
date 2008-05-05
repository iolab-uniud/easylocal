/*
 *  main.cc
 *  EasyLocalpp
 *
 *  Created by Luca Di Gaspero on 23/04/08.
 *  Copyright 2008 Università degli Studi di Udine. All rights reserved.
 *
 */

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>
#include <cppunit/ui/text/TestRunner.h>

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
