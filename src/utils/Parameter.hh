#if !defined(_PARAMETER_HH_)
#define _PARAMETER_HH_

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
#include <iostream>

/** Abstract parameter type, for containers. */
class AbstractParameter
{
  friend class Parametrized;
  friend class ParameterNotSet;
public:
  /** Reads the value of the parameter from a stream, defined in Parameter<T>. */
  virtual std::istream& Read(std::istream& is = std::cin) = 0;
  
  /** Writes the value of the parameter on a stream, defined in Parameter<T>. */
  virtual std::ostream& Write(std::ostream& os = std::cout) const = 0;
  
protected:
  
  /** Can't instantiate an AbstractParameter from the outside. */
  AbstractParameter(const std::string& cmdline_flag, const std::string& description);
  
  /** Description of the parameter, for documentation and Parametrized::ReadParameters(), Parametrized::PrintParameters(). */
  std::string description;
  
  /** Flag representing the parameter. */
  std::string cmdline_flag;
};

/** Exception called whenever a needed parameter hasn't been set. */
class ParameterNotSet : public std::exception
{
public:
  /** Constructor. 
   @param p parameter which generated the exception. 
   */
  ParameterNotSet(const AbstractParameter* p) : p_par(p) {}
  const char* what() const throw();
protected:
  /** Parameter to which the exception refers. */
  const AbstractParameter* p_par;
};

/** List of parameters, to access aggregates of parameters. */
class ParameterBox : public std::vector<AbstractParameter>
{
public:
  /** Constructor. 
   @param prefix "namespace" of the parameter (e.g. specific solver or runner)
   @param description semantics of the parameter
   */
  ParameterBox(const std::string& prefix, const std::string& description);
  
  /** Namespace of the parameter. */
  const std::string prefix;
#if defined(HAVE_LINKABLE_BOOST)
  /** Object to configure boost's parameter parser. */
  boost::program_options::options_description cl_options;
#endif
  /** List of all parameter boxes that have been instantiated. */
  static std::vector<const ParameterBox*> overall_parameters;
};

/** Concrete parameter, of generic type. */
template <typename T>
class Parameter : public AbstractParameter
{
  template <typename _T>
  friend std::istream& operator>>(std::istream& is, Parameter<_T>& p);
public:
  /** Constructor.
   @param cmdline_flag flag used to pass the parameter to the command line
   @param description semantics of the parameter
   @param parameters parameter box to which this parameter refers
  */
  Parameter(const std::string& cmdline_flag, const std::string& description, ParameterBox& parameters);
  
  /** @copydoc AbstractParameter::Read */
  virtual std::istream& Read(std::istream& is = std::cin);
  
  /** @copydoc AbstractParameter::Write */
  virtual std::ostream& Write(std::ostream& os = std::cout) const;
  
  /** Implicit cast. */
  operator T() const throw (ParameterNotSet);
  
  /** Assignment. */
  const T& operator=(const T&);
  
  /** Checks if the parameter has been set. */
  bool IsSet() const { return is_set; }
  
protected:
  
  /** True if this is set. */
  bool is_set;
  
  /** Real value of the parameter. */
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
std::istream& Parameter<T>::Read(std::istream& is)
{
  is >> value;
  return is;
}

template <typename T>
std::ostream& Parameter<T>::Write(std::ostream& os) const
{
  os << value;
  return os;
}

template <typename T>
std::istream& operator>>(std::istream& is, Parameter<T>& p)
{
  is >> p.value;
  p.is_set = true;
  
  return is;
}

// TODO: add Parameter for boolean flags
/* class Parameter<bool>
{
protected:
  bool is_set;
  FlagArgument cl_arg;
}; 
*/

/** A class representing a parametrized component of EasyLocal++. */
class Parametrized {
  
public:
  
  /** Constructor.
   @param prefix namespace of the parameters
   @param description semantics of the parameters group
  */
  Parametrized(const std::string& prefix, const std::string& description) : parameters(prefix, description) { }
  
  /** Read all parameters from an input stream (prints hints on output stream). */
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
  {
    for(auto& p : parameters)
    {
      os << "  " << p.cmdline_flag << "(" << p.description << "): " << std::endl;
      p.Read(is);
    }
  }

  /** Print all parameter values on an output stream. */
  void PrintParameters(std::ostream& os = std::cout) const
  {
    for(auto& p : parameters)
    {
      os << "  " << p.cmdline_flag << "(" << p.description << "): ";
      p.Write(os) << std::endl;
    }
  }
  
protected:
  
  ParameterBox parameters;
  
};

#endif
