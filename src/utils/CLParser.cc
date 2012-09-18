#include <utils/CLParser.hh>
#include <cstring>
#include <algorithm>
#include <typeinfo>

Argument::Argument(const std::string& fl, const std::string& al, bool req)
: flag("-" + fl), alias("-" + al), value_set(false), required(req)
{ 
  if (fl == "") 
    flag = "";
  if (al == "")
    alias = "";
  if (flag != "" && !isalpha(flag[1]))
    throw std::logic_error("Argument flag " + flag + " should be a valid argument identifier");
  if (alias != "" && !isalpha(alias[1]))
    throw std::logic_error("Argument alias " + alias + " should be a valid argument identifier");
}

Argument::Argument(const std::string& fl, const std::string& al, bool req, CLParser& cl)
: flag("-" + fl), alias("-" + al), value_set(false), required(req)
{ 
  if (fl == "") 
    flag = "";
  if (al == "")
    alias = "";
  if (flag != "" && !isalpha(flag[1]))
    throw std::logic_error("Argument flag " + flag + " should be a valid argument identifier");
  if (alias != "" && !isalpha(alias[1]))
    throw std::logic_error("Argument alias " + alias + " should be a valid argument identifier");
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
: Argument(flag, alias, required), num_of_values(0), num_of_values_read(0) {}

ArgumentGroup::ArgumentGroup(const std::string& flag, const std::string& alias, bool required, CLParser& cl)
: Argument(flag, alias, required, cl), num_of_values(0), num_of_values_read(0) {}

ArgumentGroup::ArgumentGroup(arg_list& al) 
: Argument("", "", true), num_of_values(0), num_of_values_read(0) 
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
  throw ArgumentValueNotCorrect("Could not run this version of the read method");
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

Argument& ArgumentGroup::FindArgument(const std::string& f) const
{
  arg_list::const_iterator li;
  for (li = arguments.begin(); li != arguments.end(); li++)
    if ((*li)->GetFlag() == f || (*li)->GetAlias() == f)
      return *(*li);
  throw FlagNotFound(f);
}

void ArgumentGroup::Read(const std::vector<std::string>& command_line_arguments)
{
  size_t i;
  std::string flag, value;
  arg_list::iterator pos;
  
  num_of_values_read = 0;
  i = 0;
  while (i < command_line_arguments.size())
  {
    flag = command_line_arguments[i];   
    try 
    {
      Argument& arg = FindArgument(flag);
      if (arg.IsFlagArgument())
      {
        arg.Read("");
        i++;
      }
      else
      {
        size_t size = arg.NumOfValues();
        if (i + size >= command_line_arguments.size() && arg.IsValArgument())
          throw ArgumentValueNotCorrect("Error: Value(s) for option " + flag + " not specified");
        else
          size = std::min<size_t>(size, command_line_arguments.size() - (i + 1));
        std::vector<std::string> tmp(size);
        std::string tmp_w = "";
        for (size_t j = 0; j < size; j++)
        {
          tmp[j] = command_line_arguments[i + j + 1];
          tmp_w += command_line_arguments[i + j + 1] + " ";
        }
        arg.Read(tmp);
        if (!arg.IsSet())
          throw ArgumentValueNotCorrect("Value <" + tmp_w + "> for option " + flag + " not correct");
        i += 1 + arg.NumOfValuesRead();
      }
    }
    catch (FlagNotFound e)
    {
      break;
    }
  }
  for (pos = arguments.begin(); pos != arguments.end(); pos++)
  {
    Argument& arg = **pos;
    if (arg.IsRequired() && !arg.IsSet())
      throw ArgumentNotFound("Required argument " + arg.GetFlag() + " has not been specified");
    else if (arg.IsSet())
      num_of_values_read += arg.NumOfValues() + 1;
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

// static empty CLParser
CLParser CLParser::empty;

CLParser::CLParser(int argc, char const * const argv[])
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

void CLParser::MatchArguments(bool terminate_if_fail) 
{
  if (terminate_if_fail)
  {
    try
    {
      Parse();
    }
    catch (CLParserException e)
    {
      std::cerr << e.message() << std::endl;
      std::cerr << *this << std::endl;
      exit(-1);
    }
  }
  else
    Parse();
} 

void CLParser::MatchArgument(Argument& a, bool terminate_if_fail) 
{
  if (terminate_if_fail)
  {
    try
    {
      Parse(a);
    }
    catch (std::exception e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr << *this << std::endl;
      exit(-1);
    }
  }
  else
    Parse(a);
}

void CLParser::Parse(Argument& a) 
{
  for (size_t i = 0; i < command_line_arguments.size(); i++)
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
	      size_t size = a.NumOfValues();
	      if (i + size >= command_line_arguments.size() && strstr(typeid(a).name(), "ValArgument"))
          throw ArgumentValueNotCorrect("Error: Value(s) for option " + a.GetFlag() + " not specified");
	      else
          size = std::min<size_t>(size, command_line_arguments.size() - (i + 1));
	      std::vector<std::string> tmp(size);
	      std::string tmp_w = "";
	      for (size_t j = 0; j < size; j++)
        {
          tmp[j] = command_line_arguments[i + j + 1];
          tmp_w += command_line_arguments[i + j + 1] + " ";
        }
	      a.Read(tmp);
	      if (!a.IsSet())
          throw ArgumentValueNotCorrect("Value <" + tmp_w + "> for option " + a.GetFlag() + " not correct");
        i += 1 + a.NumOfValuesRead();
	    }
    }
  }
  if (a.IsRequired() && !a.IsSet())
    throw ArgumentNotFound("Required argument " + a.GetFlag() + " has not been specified");
}  

void CLParser::Parse()
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


