
#include "CLParser.hh"
#include <cstring>
#include <algorithm>
#include "ArgumentGroup.hh"

//CLParser nullParser(0, NULL);

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


