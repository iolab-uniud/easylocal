#include "Parameter.hh"

std::vector<const ParameterBox*> ParameterBox::overall_parameters;

const char* ParameterNotSet::what() const throw()
{
  return (std::string("Parameter ") + p_par->cmdline_flag + " not set").c_str();
}