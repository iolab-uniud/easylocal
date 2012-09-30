#ifndef EasyLocal_Parameter_hh
#define EasyLocal_Parameter_hh

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif

#include <stdexcept>
#if defined(HAVE_LINKABLE_BOOST)
#include <boost/program_options/options_description.hpp>
#endif
#include <vector>
#include <string>

class AbstractParameter
{
  friend class ParameterNotSet;
protected:
  AbstractParameter(const std::string& cmdline_flag, const std::string& description);
  std::string description;
  std::string cmdline_flag;
};

class ParameterNotSet
: public std::exception
{
public:
  ParameterNotSet(const AbstractParameter* p) : p_par(p) {}
  const char* what() const throw();
protected:
  const AbstractParameter* p_par;
};

class ParameterBox : public std::vector<AbstractParameter>
{
public:
  ParameterBox(const std::string& prefix, const std::string& description);
  const std::string prefix;
#if defined(HAVE_LINKABLE_BOOST)
  boost::program_options::options_description cl_options;
#endif
  static std::vector<const ParameterBox*> overall_parameters;
};

template <typename T>
class Parameter : public AbstractParameter
{
  template <typename _T>
  friend std::istream& operator>>(std::istream& is, Parameter<_T>& p);
public:
  Parameter(const std::string& cmdline_flag, const std::string& description, ParameterBox& parameters);
  operator T() const throw (ParameterNotSet);
  const T& operator=(const T&);
  bool IsSet() const { return is_set; }
protected:
  bool is_set;
  T value;
};

template <typename T>
Parameter<T>::Parameter(const std::string& cmdline_flag, const std::string& description, ParameterBox& parameters)
: AbstractParameter(cmdline_flag, description), is_set(false)
{
  std::string flag = parameters.prefix + "::" + cmdline_flag;
#if defined(HAVE_LINKABLE_BOOST)
  parameters.cl_options.add_options()
  (flag.c_str(), boost::program_options::value<T>(&value)->notifier([this](const T&){ this->is_set = true; }), description.c_str());
#endif
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
  
  return is;
}

// TODO: add Parameter for boolean flags

// TODO: specialize for bool (i.e., flags)
/* class Parameter<bool>
{
protected:
  bool is_set;
  FlagArgument cl_arg;
}; */

class CommandLineParameters
{
public:
  static bool Parse(int argc, const char* argv[], bool check_unregistered = true);
};

#endif
