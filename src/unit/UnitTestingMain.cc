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

#include <utils/Random.hh>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>
#include <cppunit/ui/text/TestRunner.h>
#include "CLParserTest.hh"

int main(int argc, char *argv[]) 
{
	Random::Seed(0); // to ensure reproducibility of results
	
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
