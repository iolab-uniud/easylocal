/* A class for command line flags. */

#ifndef _FLAGARGUMENT_HH
#define _FLAGARGUMENT_HH

#include "Argument.hh"

class FlagArgument : public Argument
{
public:
  FlagArgument(const std::string& flag, bool required)
    : Argument(flag, required) {}
  void read(const std::string& val)
  { value_set = true; }
	void read(const std::vector<std::string>& val)
	{ value_set = true; }
  unsigned int numOfValues() const
  { return 0; }
};

#endif

