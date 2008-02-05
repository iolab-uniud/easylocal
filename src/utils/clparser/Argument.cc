#include "Argument.hh"
#include "CLParser.hh"
#include "ArgumentGroup.hh"

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
