// $Id: OutputManagerTest.hh 201 2008-05-18 19:47:38Z digasper $
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

#if !defined(_CLPARSERTEST_HH_)
#define _CLPARSERTEST_HH_

#include <cppunit/extensions/HelperMacros.h>
#include <unit/TestUtils.hh>
#include <utils/CLParser.hh>

class CLParserTest : public CPPUNIT_NS::TestFixture {
	CPPUNIT_TEST_SUITE(CLParserTest);
	CPPUNIT_TEST(testValArguments1);
  CPPUNIT_TEST(testValArguments2);
  CPPUNIT_TEST(testValArgumentMultiLength1);
  CPPUNIT_TEST(testValArgumentMultiLength2);
  CPPUNIT_TEST(testValArgumentMultiLength3);
  CPPUNIT_TEST(testFlagArguments1);
  CPPUNIT_TEST(testFlagArguments2);
  CPPUNIT_TEST(testMixedArguments1);
  CPPUNIT_TEST(testMixedArguments2);
  CPPUNIT_TEST(testMixedArguments3);
  CPPUNIT_TEST(testGroupArguments1);
  CPPUNIT_TEST(testGroupArguments2);
  CPPUNIT_TEST(testGroupArguments3);
	CPPUNIT_TEST_SUITE_END();
protected:
public:	  
	void setUp()
	{}
	
	void tearDown()
	{}
  	
	void testValArguments1() 
	{ // Tests presence of required argument on the command-line and the absence of the optional argument
    char const* argv[] = { "dummy_command_name", "-r", "required_value" }; 
    CLParser cl(3, argv);
    ValArgument<std::string> areq("required", "r", true, cl);
    ValArgument<int> aopt("optional", "o", false, cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), areq.GetValue(), std::string("required_value"));
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in the command-line was found", __FILE__, __LINE__), !aopt.IsSet());
  }

	void testValArguments2()   
  { // Tests presence of both the required argument and the optional argument on the command-line
    char const* argv[] = { "dummy_command_name", "-r", "required_value", "-o", "3" }; 
    CLParser cl(5, argv);
    ValArgument<std::string> areq("required", "r", true, cl);
    ValArgument<int> aopt("optional", "o", false, cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), areq.GetValue(), std::string("required_value"));
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in the command-line was not found", __FILE__, __LINE__), aopt.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Optional argument in the command-line has not the expected value", __FILE__, __LINE__), aopt.GetValue(), 3);
	}
  
  void testValArgumentMultiLength1()   
  { // Tests multi-length required arguments (and more optional arguments)
    char const* argv[] = { "dummy_command_name", "-r", "required_value1", "required_value2", "-o1", "3" }; 
    CLParser cl(6, argv);
    ValArgument<std::string, 2> areq("required", "r", true, cl);
    ValArgument<int> aopt1("optional1", "o1", false, cl), aopt2("optional2", "o2", false, cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), std::string("required_value1"), areq.GetValue(0));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), std::string("required_value2"), areq.GetValue(1));
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in the command-line was not found", __FILE__, __LINE__), aopt1.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Optional argument in the command-line has not the expected value", __FILE__, __LINE__), 3, aopt1.GetValue());
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in the command-line was found", __FILE__, __LINE__), !aopt2.IsSet());
	}
  
  void testValArgumentMultiLength2()   
  { // Tests multi-length required arguments (and more optional arguments)
    char const* argv[] = { "dummy_command_name", "-o1", "3", "-r", "required_value1", "required_value2" }; 
    CLParser cl(6, argv);
    ValArgument<std::string, 2> areq("required", "r", true, cl);
    ValArgument<int> aopt1("optional1", "o1", false, cl), aopt2("optional2", "o2", false, cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), std::string("required_value1"), areq.GetValue(0));
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), std::string("required_value2"), areq.GetValue(1));
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in the command-line was not found", __FILE__, __LINE__), aopt1.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Optional argument in the command-line has not the expected value", __FILE__, __LINE__), 3, aopt1.GetValue());
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in the command-line was found", __FILE__, __LINE__), !aopt2.IsSet());
	}
  
  void testValArgumentMultiLength3()   
  { // Tests multi-length required arguments not completely specified
    char const* argv[] = { "dummy_command_name", "-r", "required_value1", "-o", "3" }; 
    CLParser cl(5, argv);
    ValArgument<std::string, 2> areq("required", "r", true, cl);
    ValArgument<int> aopt1("optional", "o", false, cl);
    CPPUNIT_ASSERT_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false), ArgumentValueNotCorrect);
	}
  
  void testFlagArguments1()   
  { // Tests flag arguments (absence)
    char const* argv[] = { "dummy_command_name" }; 
    CLParser cl(2, argv);
    FlagArgument aflag("flag", "f", cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Flag argument in the command-line was found", __FILE__, __LINE__), !aflag.IsSet());
	}
  
  void testFlagArguments2()   
  { // Tests flag arguments (presence)
    char const* argv[] = { "dummy_command_name", "-f" }; 
    CLParser cl(2, argv);
    FlagArgument aflag("flag", "f", cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Flag argument in the command-line was not found", __FILE__, __LINE__), aflag.IsSet());
	}
  
  void testMixedArguments1()   
  { // Tests mixed flag and valued arguments
    char const* argv[] = { "dummy_command_name", "-f", "-r", "required_value" }; 
    CLParser cl(4, argv);
    ValArgument<std::string> areq("required", "r", true, cl);
    FlagArgument aflag("flag", "f", cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Flag argument in the command-line was not found", __FILE__, __LINE__), aflag.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__),  std::string("required_value"), areq.GetValue());
	}
  
  void testMixedArguments2()   
  { // Tests mixed flag and valued arguments
    char const* argv[] = { "dummy_command_name", "-r", "required_value", "-f" }; 
    CLParser cl(4, argv);
    ValArgument<std::string> areq("required", "r", true, cl);
    FlagArgument aflag("flag", "f", cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Flag argument in the command-line was not found", __FILE__, __LINE__), aflag.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), std::string("required_value"), areq.GetValue());
	}
  
  void testMixedArguments3()   
  { // Tests mixed flag and valued arguments (absence of flag)
    char const* argv[] = { "dummy_command_name", "-r", "required_value" }; 
    CLParser cl(3, argv);
    ValArgument<std::string> areq("required", "r", true, cl);
    FlagArgument aflag("flag", "f", cl);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Flag argument in the command-line was found", __FILE__, __LINE__), !aflag.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), std::string("required_value"), areq.GetValue());
	}
  
  void testGroupArguments1()   
  { // Tests argument groups (just one, required)
    char const* argv[] = { "dummy_command_name", "-g", "-r", "required_value" }; 
    CLParser cl(4, argv);
    ArgumentGroup agrp("group", "g", true, cl);
    ValArgument<std::string> areq("required", "r", true);
    agrp.AddArgument(areq);
    ValArgument<int> aopt("optional", "o", false);
    agrp.AddArgument(aopt);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Required group in the command-line was not found", __FILE__, __LINE__), agrp.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in the command-line was not found", __FILE__, __LINE__), areq.IsSet());
    CPPUNIT_ASSERT_EQUAL_MESSAGE(stringify("Required argument in the command-line has not the expected value", __FILE__, __LINE__), std::string("required_value"), areq.GetValue());
	}
  
  void testGroupArguments2()   
  { // Tests argument groups (just one, optional)
    char const* argv[] = { "dummy_command_name" }; 
    CLParser cl(1, argv);
    ArgumentGroup agrp("group", "g", false, cl);
    ValArgument<std::string> areq("required", "r", true);
    agrp.AddArgument(areq);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional group in the command-line was found", __FILE__, __LINE__), !agrp.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in an optional group was found", __FILE__, __LINE__), !areq.IsSet());
	}

  void testGroupArguments3()   
  { // Tests argument groups (just one, optional, with two levels required)
    char const* argv[] = { "dummy_command_name", "-g", "-gr", "required_value", "-o", "3"}; 
    CLParser cl(6, argv);
    ArgumentGroup agrp("group", "g", false, cl);
    ValArgument<std::string> agreq("group_required", "gr", true);
    agrp.AddArgument(agreq);
    ValArgument<int> aopt1("group_optional", "go", false), aopt2("optional", "o", false, cl);
    agrp.AddArgument(aopt1);
    CPPUNIT_ASSERT_NO_THROW_MESSAGE(stringify("Match argument failed", __FILE__, __LINE__), cl.MatchArguments(false));
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional group in the command-line was not found", __FILE__, __LINE__), agrp.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Required argument in an optional group was not found", __FILE__, __LINE__), agreq.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in the command-line was not found", __FILE__, __LINE__), aopt2.IsSet());
    CPPUNIT_ASSERT_MESSAGE(stringify("Optional argument in an optional group was found", __FILE__, __LINE__), !aopt1.IsSet());
	}
  
  
};

CPPUNIT_TEST_SUITE_REGISTRATION(CLParserTest);

#endif // _CLPARSERTEST_HH_

