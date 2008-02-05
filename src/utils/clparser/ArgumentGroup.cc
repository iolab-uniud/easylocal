#include "ArgumentGroup.hh"
#include <algorithm>


ArgumentGroup::ArgumentGroup(const std::string& flag, const std::string& alias, bool required)
: Argument(flag, alias, required), num_of_values(0) {}

ArgumentGroup::ArgumentGroup(const std::string& flag, const std::string& alias, bool required, CLParser& cl)
: Argument(flag, alias, required, cl), num_of_values(0) {}

ArgumentGroup::ArgumentGroup(arg_list& al) 
: Argument("", "", true), num_of_values(0) 
{
	for (arg_list::const_iterator li = al.begin(); li != al.end(); li++)
		AddArgument(**li);
}

void ArgumentGroup::AddArgument(Argument& a)
{
	arguments.push_back(&a); 
	num_of_values += a.NumOfValues() + 1;
}

void ArgumentGroup::Read(const std::string& val) 
{
	throw std::logic_error("Could not run this version of the read method");
}

void ArgumentGroup::PrintUsage(std::ostream& os, unsigned int tabs) const
{
	Argument::PrintUsage(os, tabs);
	for (arg_list::const_iterator li = arguments.begin(); li != arguments.end(); li++) 
	{
		os << std::endl;
		(*li)->PrintUsage(os, tabs + 1);
	}
}

Argument& ArgumentGroup::FindArgument(const std::string& f) const throw (std::logic_error)
{
	arg_list::const_iterator li;
	for (li = arguments.begin(); li != arguments.end(); li++)
		if ((*li)->GetFlag() == f || (*li)->GetAlias() == f)
			return *(*li);
	throw std::logic_error("Error: Option " + f + " not supported");
}

void ArgumentGroup::Read(const std::vector<std::string>& command_line_arguments)
{
	unsigned int i;
  std::string flag, value;
	arg_list::iterator pos;
  Argument* arg;
  
  i = 0;
  while (i < command_line_arguments.size())
  {
    flag = command_line_arguments[i];
		arg = &FindArgument(flag);
    if (strstr(typeid(*arg).name(), "FlagArgument"))
    {
      arg->Read("");
      i++;
    }
    else
    {
      unsigned int size = arg->NumOfValues();
			if (i + size >= command_line_arguments.size() && strstr(typeid(*arg).name(), "ValArgument"))
        throw std::logic_error("Error: Value(s) for option " + flag + " not specified");
			else
				size = std::min<unsigned int>(size, command_line_arguments.size() - (i + 1));
			std::vector<std::string> tmp(size);
			std::string tmp_w = "";
      for (unsigned int j = 0; j < size; j++)
			{
      	tmp[j] = command_line_arguments[i + j + 1];
				tmp_w += command_line_arguments[i + j + 1] + " ";
			}
      arg->Read(tmp);
      if (!arg->IsSet())
        throw std::logic_error("Error: Value <" + tmp_w + "> for option " + flag + " not correct");
      i += size + 1;
    }
  }
  for (pos = arguments.begin(); pos != arguments.end(); pos++)
  {
    arg = *pos;
    if (arg->IsRequired() && !arg->IsSet())
      throw std::logic_error("Error: Required option " + arg->GetFlag() + " has not been specified");
  }	
	value_set = true;
}

