#include <utils/CLParser.hh>
#include <cstring>
#include <algorithm>

Argument::Argument(const std::string& fl, const std::string& al, bool req)
  : flag("-" + fl), alias("-" + al), value_set(false), required(req)
{ 
  if (fl == "") 
    flag = "";
  if (al == "")
    alias = "";
}

Argument::Argument(const std::string& fl, const std::string& al, bool req, CLParser& cl)
  : flag("-" + fl), alias("-" + al), value_set(false), required(req)
{ 
  if (fl == "") 
    flag = "";
  if (al == "")
    alias = "";
  cl.AddArgument(*this); 
}	

void Argument::PrintUsage(std::ostream& os, unsigned int tabs) const
{
  for (unsigned int i = 0; i < tabs; i++)
    os << "  ";
  if (GetAlias() != "")
    os << GetAlias() << "  ";
  os << GetFlag();
  if (IsRequired())
    os << "*";
}

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



std::ostream& operator<<(std::ostream& os, const CLParser& cl)
{
  os << "Usage: " << cl.command_name << std::endl;
  os << "Parameters: " << std::endl;
	
  for (CLParser::arg_list::const_iterator li = cl.arguments.begin(); li != cl.arguments.end(); li++)
    {
      (*li)->PrintUsage(os);
      os << std::endl;
    }
	
  return os;
}

CLParser::CLParser(int argc, char * const argv[])
{
  if (argc > 0)
    {
      command_name = argv[0];
      command_line_arguments.clear();
      for (int i = 1; i < argc; i++)
	command_line_arguments.push_back(argv[i]);
    }
}

CLParser::~CLParser()
{}

void CLParser::AddArgument(Argument& a)
{ 
  arguments.push_back(&a); 
} 

void CLParser::MatchArguments() 
{
  try
    {
      Parse();
    }
  catch (std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr << *this << std::endl;
      exit(-1);
    }
} 

void CLParser::MatchArgument(Argument& a) 
{
  for (unsigned int i = 0; i < command_line_arguments.size(); i++)
    {
      const std::string& flag = command_line_arguments[i];
      if (flag == a.GetFlag() || flag == a.GetAlias())
	{
	  if (strstr(typeid(a).name(), "FlagArgument"))
	    {
	      a.Read("");
	      i++;
	    }
	  else
	    {
	      unsigned int size = a.NumOfValues();
	      if (i + size >= command_line_arguments.size() && strstr(typeid(a).name(), "ValArgument"))
		throw std::logic_error("Error: Value(s) for option " + a.GetFlag() + " not specified");
	      else
		size = std::min<unsigned int>(size, command_line_arguments.size() - (i + 1));
	      std::vector<std::string> tmp(size);
	      std::string tmp_w = "";
	      for (unsigned int j = 0; j < size; j++)
		{
		  tmp[j] = command_line_arguments[i + j + 1];
		  tmp_w += command_line_arguments[i + j + 1] + " ";
		}
	      a.Read(tmp);
	      if (!a.IsSet())
		throw std::logic_error("Error: Value <" + tmp_w + "> for option " + a.GetFlag() + " not correct");
	      i += size + 1;
	    }
	}
    }
  if (a.IsRequired() && !a.IsSet())
    throw std::logic_error("Error: Required option " + a.GetFlag() + " has not been specified");
}

void CLParser::Parse() throw (std::logic_error)
{
  for (unsigned int i = 0; i < command_line_arguments.size(); i++)
    {
      const std::string& flag = command_line_arguments[i];
      if (flag == "-help" || flag == "-h")
	throw std::logic_error("Command help:");
    }
	
  ArgumentGroup ag(arguments);
  ag.Read(command_line_arguments);
}


