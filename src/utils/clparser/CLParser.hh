/* A command line parser class */

#ifndef _CLPARSER_HH
#define _CLPARSER_HH

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <list>
#include "Argument.hh"

class CLParser // : public CLParserInterface
{
	friend std::ostream& operator<<(std::ostream& os, const CLParser& cl);
public:
  CLParser(int argc, char * const argv[]);
  void AddArgument(Argument& a);
  void MatchArguments();
	void MatchArgument(Argument& a);
  virtual ~CLParser();
protected:
	typedef std::list<Argument*> arg_list;
  void Parse() throw (std::logic_error);
  std::string command_name;
  std::vector<std::string> command_line_arguments;
	arg_list arguments;
};

std::ostream& operator<<(std::ostream& os, const CLParser& cl);

//extern CLParser nullCL;

#endif
