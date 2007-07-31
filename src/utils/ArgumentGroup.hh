/* A class for group of command line arguments. */

#ifndef _ARGUMENTGROUP_HH
#define _ARGUMENTGROUP_HH

#include "Argument.hh"
#include <string>
#include <vector>
#include <list>
#include <stdexcept>

class ArgumentGroup : public Argument
{
public:
	typedef std::list<Argument*> arg_list;
  ArgumentGroup(const std::string& flag, bool required);
  void read(const std::string& val);
	void read(const std::vector<std::string>& val);
	void addArgument(Argument& a);
  unsigned int numOfValues() const
	{ return num_of_values; }
	void printUsage(std::ostream& os, unsigned int tabs = 0) const;
protected:
	Argument* findArgument(const std::string&) const throw (std::runtime_error);
	arg_list arguments;
	unsigned int num_of_values;
};

#endif

