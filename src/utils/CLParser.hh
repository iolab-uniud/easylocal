/* A command line parser class */

#ifndef _CLPARSER_HH
#define _CLPARSER_HH

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include "Argument.hh"
#include "ArgumentGroup.hh"
#include "ValArgument.hh"
#include "FlagArgument.hh"

class CLParser 
{
	friend std::ostream& operator<<(std::ostream& os, const CLParser& cl);
public:
  CLParser(int argc, char * const argv[]);
  void addArgument(Argument& a);
  void matchArguments();
protected:
  void parse() throw (std::runtime_error);
//	Argument* findArgument(const std::string& f) const throw (std::runtime_error);
  std::string command_name;
  std::vector<std::string> command_line_arguments;
  ArgumentGroup main_arguments_group;
};

std::ostream& operator<<(std::ostream& os, const CLParser& cl);

#endif
