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
	friend class CLParser;
public:
	ArgumentGroup(const std::string& flag, const std::string& alias, bool required);
	ArgumentGroup(const std::string& flag, const std::string& alias, bool required, CLParser& cl);
  void Read(const std::string& val);
	void Read(const std::vector<std::string>& val);
	void AddArgument(Argument& a);
  unsigned int NumOfValues() const
	{ return num_of_values; }
	void PrintUsage(std::ostream& os, unsigned int tabs = 0) const;
protected:
	typedef std::list<Argument*> arg_list;
	ArgumentGroup(arg_list& al);
	Argument& FindArgument(const std::string&) const throw (std::logic_error);
	arg_list arguments;
	unsigned int num_of_values;
};

#endif

