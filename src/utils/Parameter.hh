#ifndef EasyLocal_Parameter_hh
#define EasyLocal_Parameter_hh

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif

#include <stdexcept>
#if defined(HAVE_BOOST)
#include <boost/program_options/options_description.hpp>
#endif
#include <vector>

class AbstractParameter
{
  friend class ParameterNotSet;
protected:
  AbstractParameter(const std::string& cmdline_flag, const std::string& description);
  std::string description;
  std::string cmdline_flag;
};

AbstractParameter::AbstractParameter(const std::string& cf, const std::string& d)
: description(d), cmdline_flag(cf)
{}

class ParameterNotSet
: public std::exception
{
public:
  ParameterNotSet(const AbstractParameter* p) : p_par(p) {}
  const char* what() const throw();
protected:
  const AbstractParameter* p_par;
};

const char* ParameterNotSet::what() const throw()
{
  return (std::string("Parameter ") + p_par->cmdline_flag + " not set").c_str();
}

class ParameterBox : public std::vector<AbstractParameter>
{
public:
  ParameterBox(const std::string& prefix, const std::string& description);
  const std::string prefix;
  boost::program_options::options_description cl_options;
  static std::vector<const ParameterBox*> overall_parameters;
};


ParameterBox::ParameterBox(const std::string& p, const std::string& description) : prefix(p), cl_options(description)
{
  overall_parameters.push_back(this);
}


template <typename T>
class Parameter : public AbstractParameter
{
  friend std::istream& operator>>(std::istream& is, Parameter<T>& p);
public:
  Parameter(const std::string& description, const std::string& cmdline_flag, ParameterBox& parameters);
  operator T() const throw (ParameterNotSet);
  const T& operator=(const T&);
protected:
  bool is_set;
  T value;
};

template <typename T>
Parameter<T>::Parameter(const std::string& cmdline_flag, const std::string& description, ParameterBox& parameters)
: AbstractParameter(cmdline_flag, description), is_set(false)
{
  std::string flag = parameters.prefix + "::" + cmdline_flag;
  parameters.cl_options.add_options()
  (flag.c_str(), boost::program_options::value<T>(&value)->notifier([this](const T&){ this->is_set = true; }), description.c_str());
}

template <typename T>
Parameter<T>::operator T() const throw (ParameterNotSet)
{
  if (!is_set)
    throw new ParameterNotSet(this);
  return value;
}

template <typename T>
const T& Parameter<T>::operator=(const T& v)
{
  is_set = true;
  value = v;
  return value;
}

template <typename T>
std::istream& operator>>(std::istream& is, Parameter<T>& p)
{
  is >> p.value;
  p.is_set = true;
}


// TODO: specialize for bool (i.e., flags)
/* class Parameter<bool>
{
protected:
  bool is_set;
  FlagArgument cl_arg;
}; */

#endif
