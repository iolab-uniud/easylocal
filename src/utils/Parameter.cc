#include "Parameter.hh"

std::vector<const ParameterBox*> ParameterBox::overall_parameters;

const char* ParameterNotSet::what() const throw()
{
  return (std::string("Parameter ") + p_par->cmdline_flag + " not set").c_str();
}


ParameterBox::ParameterBox(const std::string& p, const std::string& description) : prefix(p)
#if defined(HAVE_BOOST)
, cl_options(description)
#endif
{
  overall_parameters.push_back(this);
}
