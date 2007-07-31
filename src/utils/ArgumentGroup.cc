#include "ArgumentGroup.hh"
#include <algorithm>

ArgumentGroup::ArgumentGroup(const std::string& flag, bool required)
: Argument(flag, required), num_of_values(0) {}

void ArgumentGroup::addArgument(Argument& a)
{
	arguments.push_back(&a);
	num_of_values += a.numOfValues() + 1;
}

void ArgumentGroup::read(const std::string& val) 
{
	throw new std::runtime_error("Could not run this version of the read method");
}

void ArgumentGroup::printUsage(std::ostream& os, unsigned int tabs) const
{
	Argument::printUsage(os, tabs);
	for (arg_list::const_iterator li = arguments.begin(); li != arguments.end(); li++) 
	{
		os << std::endl;
		(*li)->printUsage(os, tabs + 1);
	}
}

Argument* ArgumentGroup::findArgument(const std::string& f) const throw (std::runtime_error)
{
	arg_list::const_iterator li;
	for (li = arguments.begin(); li != arguments.end(); li++)
		if ((*li)->getFlag() == f || (*li)->getAlias() == f)
			return *li;
	throw std::runtime_error("Error: Option " + f + " not supported");
}

void ArgumentGroup::read(const std::vector<std::string>& command_line_arguments)
{
	unsigned int i;
  std::string flag, value;
	arg_list::iterator pos;
  Argument* arg;
  
  i = 0;
  while (i < command_line_arguments.size())
  {
    flag = command_line_arguments[i];
		arg = findArgument(flag);
    if (strstr(typeid(*arg).name(), "FlagArgument"))
    {
      arg->read("");
      i++;
    }
    else
    {
      int size = arg->numOfValues();
			if (i + size >= command_line_arguments.size() && strstr(typeid(*arg).name(), "ValArgument"))
        throw std::runtime_error("Error: Value(s) for option " + flag + " not specified");
			else
				size = std::min<unsigned int>(size, command_line_arguments.size() - (i + 1));
			std::vector<std::string> tmp(size);
			std::string tmp_w = "";
      for (unsigned int j = 0; j < size; j++)
			{
      	tmp[j] = command_line_arguments[i + j + 1];
				tmp_w += command_line_arguments[i + j + 1] + " ";
			}
      arg->read(tmp);
      if (!arg->isSet())
        throw std::runtime_error("Error: Value <" + tmp_w + "> for option " + flag + " not correct");
      i += size + 1;
    }
  }
  for (pos = arguments.begin(); pos != arguments.end(); pos++)
  {
    arg = *pos;
    if (arg->isRequired() && !arg->isSet())
      throw std::runtime_error("Error: Required option " + arg->getFlag() + " has not been specified");
  }	
	value_set = true;
}

