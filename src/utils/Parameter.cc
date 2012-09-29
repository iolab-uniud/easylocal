#include "Parameter.hh"

AbstractParameter::AbstractParameter(const std::string& cf, const std::string& d)
: description(d), cmdline_flag(cf)
{}

const char* ParameterNotSet::what() const throw()
{
  return (std::string("Parameter ") + p_par->cmdline_flag + " not set").c_str();
}

std::vector<const ParameterBox*> ParameterBox::overall_parameters;

ParameterBox::ParameterBox(const std::string& p, const std::string& description) : prefix(p)
#if defined(HAVE_LINKABLE_BOOST)
, cl_options(description)
#endif
{
  overall_parameters.push_back(this);
}

