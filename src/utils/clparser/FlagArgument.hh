/* A class for command line flags. */

#ifndef _FLAGARGUMENT_HH
#define _FLAGARGUMENT_HH

#include "Argument.hh"

class CLParser;
class ArgumentGroup;

class FlagArgument : public Argument
{
public:
	FlagArgument(const std::string& flag, const std::string& alias, bool required)
	: Argument(flag, alias, required) {}
	FlagArgument(const std::string& flag, const std::string& alias, bool required, CLParser& cl)
	: Argument(flag, alias, required, cl) {}
  void Read(const std::string& val)
  { value_set = true; }
	void Read(const std::vector<std::string>& val)
	{ value_set = true; }
  unsigned int NumOfValues() const
  { return 0; }
};

#endif

