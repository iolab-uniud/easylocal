
#include "CLParser.hh"
#include <cstring>
#include <algorithm>

std::ostream& operator<<(std::ostream& os, const CLParser& cl)
{
  os << "Usage: " << cl.command_name << std::endl;

	cl.main_arguments_group.printUsage(os);

  return os;
}

CLParser::CLParser(int argc, char * const argv[])
: main_arguments_group("Parameters:", true)
{
  command_name = argv[0];
  command_line_arguments.clear();
  for (unsigned int i = 1; i < argc; i++)
    command_line_arguments.push_back(argv[i]);
}

void CLParser::addArgument(Argument& a)
{ 
	main_arguments_group.addArgument(a); 
}

void CLParser::parse() throw (std::runtime_error)
{
	for (unsigned int i = 0; i < command_line_arguments.size(); i++)
	{
		const std::string& flag = command_line_arguments[i];
		if (flag == "-help" || flag == "-h")
			throw std::runtime_error("Command help:");
	}
	main_arguments_group.read(command_line_arguments);
}

void CLParser::matchArguments() 
{
  try
{
	parse();
}
catch (std::runtime_error e)
{
	std::cerr << e.what() << std::endl;
	std::cerr << *this << std::endl;
	exit(-1);
}
}
