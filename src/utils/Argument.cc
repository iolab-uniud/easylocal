#include "Argument.hh"

void Argument::printUsage(std::ostream& os, unsigned int tabs) const
{
	for (unsigned int i = 0; i < tabs; i++)
		os << "  ";
	if (getAlias() != "")
		os << getAlias() << "  ";
	os << getFlag();
	if (isRequired())
		os << "*";
}
