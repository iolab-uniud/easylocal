#pragma once

#include <stdexcept>
#include <list>
#include <string>
#include <iostream>
#include <memory>
#include <sstream>
#include "easylocal/utils/types.hh"

#include "boost/program_options/options_description.hpp"

namespace EasyLocal
{

namespace Core
{

/** Abstract parameter type, for containers. */
class AbstractParameter
{
  friend class Parametrized;
  friend class ParameterNotSet;
  friend class ParameterNotValid;

public:
  /** Reads the value of the parameter from a stream, defined in Parameter<T>. */
  virtual std::istream &Read(std::istream &is = std::cin) = 0;

  /** Writes the value of the parameter on a stream, defined in Parameter<T>. */
  virtual std::ostream &Write(std::ostream &os = std::cout) const = 0;

  /** Checks if the parameter has been set. */
  bool IsSet() const { return is_set; }

  /** Checks if the parameter is valid. */
  bool IsValid() const { return is_valid; }

  /** To print out values. */
  virtual std::string ToString() const = 0;

protected:
  AbstractParameter();

  /** Can't instantiate an AbstractParameter from the outside. */
  AbstractParameter(const std::string &cmdline_flag, const std::string &description);

  /** Description of the parameter, for documentation and Parametrized::ReadParameters(), Parametrized::PrintParameters(). */
  std::string description;

  /** Flag representing the parameter. */
  std::string cmdline_flag;

  /** True if this is set. */
  bool is_set;

  /** True if it has been correctly instantiated. */
  bool is_valid;

  virtual void CopyValue(const AbstractParameter &ap) = 0;
};

/** Exception called whenever a needed parameter hasn't been set. */
class ParameterNotSet : public std::logic_error
{
public:
  /** Constructor.
       @param p parameter which generated the exception.
       */
  ParameterNotSet(const AbstractParameter &p) : std::logic_error(std::string("Parameter ") + p.cmdline_flag + " not set") {}
};

/** Exception called whenever a needed parameter is not valid (i.e., properly created/attached). */
class ParameterNotValid : public std::logic_error
{
public:
  /** Constructor.
       @param p parameter which generated the exception.
       */
  ParameterNotValid(const AbstractParameter &p) : std::logic_error(std::string("Parameter ") + p.cmdline_flag + " not valid") {}
};

/** List of parameters, to access aggregates of parameters. */
class ParameterBox : public std::vector<AbstractParameter *>
{
public:
  /** Constructor.
       @param prefix "namespace" of the parameter (e.g. specific solver or runner)
       @param description semantics of the parameter
       */
  ParameterBox(const std::string &prefix, const std::string &description);

  /** Namespace of the parameter. */
  const std::string prefix;
  /** Object to configure boost's parameter parser. */
  boost::program_options::options_description cl_options;
  /** List of all parameter boxes that have been instantiated. */
  static std::list<const ParameterBox *> overall_parameters;
};

/** Concrete parameter of generic type. */
template <typename T>
class BaseParameter : public AbstractParameter
{
  template <typename _T>
  friend std::istream &operator>>(std::istream &is, BaseParameter<_T> &p);
  template <typename _T>
  friend bool operator==(const BaseParameter<_T> &, const _T &);
  friend class IncorrectParameterValue;

protected:
  using AbstractParameter::AbstractParameter;

public:
  /** @copydoc AbstractParameter::Read */
  virtual std::istream &Read(std::istream &is = std::cin);

  /** @copydoc AbstractParameter::Write */
  virtual std::ostream &Write(std::ostream &os = std::cout) const;

  /** @copydoc AbstractParameter::ToString */
  virtual std::string ToString() const
  {
    if (!is_valid)
    {
      throw ParameterNotValid(*this);
    }
    std::stringstream ss;
    ss << this->value;
    return ss.str();
  }

  virtual void CopyValue(const AbstractParameter &ap)
  {
    const BaseParameter<T> &tp = dynamic_cast<const BaseParameter<T> &>(ap);
    this->value = tp.value;
    this->is_set = tp.is_set;
    this->is_valid = tp.is_valid;
  }

  /** Implicit cast. */
  operator T() const;

  /** Assignment. */
  const T &operator=(const T &);

protected:
  /** Actual value of the parameter. */
  T value;
};

template <typename T>
class Parameter : public BaseParameter<T>
{
public:
  Parameter()
  {
    this->is_valid = false;
  }
  /** Constructor.
           @param cmdline_flag flag used to pass the parameter to the command line
           @param description semantics of the parameter
           @param parameters parameter box to which this parameter refers
           */
  Parameter(const std::string &cmdline_flag, const std::string &description, ParameterBox &parameters)
      : BaseParameter<T>(cmdline_flag, description)
  {
    std::string flag = parameters.prefix + "::" + cmdline_flag;
    parameters.push_back(this);
    parameters.cl_options.add_options()(flag.c_str(), boost::program_options::value<T>(&this->value)->notifier([this](const T &) { this->is_set = true; }), description.c_str());
    this->is_valid = true;
  }

  virtual void operator()(const std::string &cmdline_flag, const std::string &description, ParameterBox &parameters)
  {
    this->cmdline_flag = parameters.prefix + "::" + cmdline_flag;
    this->description = description;
    parameters.push_back(this);
    parameters.cl_options.add_options()(this->cmdline_flag.c_str(), boost::program_options::value<T>(&this->value)->notifier([this](const T &) { this->is_set = true; }), description.c_str());
    this->is_valid = true;
  }

  const T &operator=(const T &v)
  {
    if (!this->is_valid)
      throw ParameterNotValid(*this);
    this->is_set = true;
    this->value = v;
    return this->value;
  }
};

template <>
class Parameter<bool> : public BaseParameter<bool>
{
public:
  Parameter()
  {
    this->is_valid = false;
  }

  Parameter(const std::string &cmdline_flag, const std::string &description, ParameterBox &parameters)
      : BaseParameter<bool>(cmdline_flag, description)
  {
    std::string flag = parameters.prefix + "::" + cmdline_flag;
    parameters.cl_options.add_options()(("enable-" + flag).c_str(), boost::program_options::value<std::string>()->implicit_value("true")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = true; }), "")(("disable-" + flag).c_str(), boost::program_options::value<std::string>()->implicit_value("false")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = false; }),
                                                                                                                                                                                                         ("[enable/disable] " + description).c_str());
  }

  virtual void operator()(const std::string &cmdline_flag, const std::string &description, ParameterBox &parameters)
  {
    this->cmdline_flag = parameters.prefix + "::" + cmdline_flag;
    this->description = description;
    parameters.push_back(this);
    parameters.cl_options.add_options()(("enable-" + cmdline_flag).c_str(), boost::program_options::value<std::string>()->implicit_value("true")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = true; }), "")(("disable-" + cmdline_flag).c_str(), boost::program_options::value<std::string>()->implicit_value("false")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = false; }),
                                                                                                                                                                                                                 ("[enable/disable] " + description).c_str());
    this->is_valid = true;
  }

  const bool &operator=(const bool &v)
  {
    if (!this->is_valid)
      throw ParameterNotValid(*this);
    this->is_set = true;
    this->value = v;
    return this->value;
  }
};

class IncorrectParameterValue
    : public std::logic_error
{
public:
  template <typename T>
  IncorrectParameterValue(const Parameter<T> &p, std::string desc);
  virtual const char *what() const throw();
  virtual ~IncorrectParameterValue() throw();

protected:
  std::string message;
};

template <typename T>
BaseParameter<T>::operator T() const
{
  if (!is_valid)
    throw ParameterNotValid(*this);
  if (!is_set)
    throw ParameterNotSet(*this);
  return value;
}

template <typename T>
std::istream &BaseParameter<T>::Read(std::istream &is)
{
  if (!is_valid)
    throw ParameterNotValid(*this);
  std::string in;
  std::getline(is, in);

  if (in.size())
  {
    std::stringstream ss;
    ss.str(in);
    ss >> *this;
  }
  else
  {
    is_set = true;
  }

  return is;
}

template <typename T>
std::ostream &BaseParameter<T>::Write(std::ostream &os) const
{
  if (!this->is_valid)
    throw ParameterNotValid(*this);
  if (!is_set)
    throw ParameterNotSet(*this);
  os << value;
  return os;
}

template <typename T>
std::istream &operator>>(std::istream &is, BaseParameter<T> &p)
{
  if (!p.is_valid)
    throw ParameterNotValid(p);
  is >> p.value;
  p.is_set = true;

  return is;
}

template <typename T>
bool operator==(const BaseParameter<T> &t1, const T &t2)
{
  return t1.value == t2;
}

bool operator==(const BaseParameter<std::string> &s1, const char *s2);

template <typename T>
IncorrectParameterValue::IncorrectParameterValue(const Parameter<T> &p, std::string desc)
    : std::logic_error("Incorrect parameter value")
{
  std::ostringstream os;
  os << "Parameter " << p.cmdline_flag << " set to incorrect value " << p.value << " (" << desc << ")";
  message = os.str();
}

class CommandLineParameters
{
public:
  static bool Parse(int argc, const char *argv[], bool check_unregistered = true, bool silent = false);
};

/** A class representing a parametrized component of EasyLocal++. */
class Parametrized
{
  friend bool CommandLineParameters::Parse(int argc, const char *argv[], bool check_unregistered, bool silent);

public:
  /** Constructor.
           @param prefix namespace of the parameters
           @param description semantics of the parameters group
           */
  Parametrized(const std::string &prefix, const std::string &description) : parameters(prefix, description), parameters_registered(false)
  {
    overall_parametrized.push_back(this);
  }

  static void RegisterParameters()
  {
    for (auto p : overall_parametrized)
      p->_RegisterParameters();
  }

  /** Read all parameters from an input stream (prints hints on output stream). */
  virtual void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout)
  {
    is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    for (auto p : this->parameters)
    {
      os << "  " << p->description << (p->IsSet() ? (std::string(" (def.: ") + p->ToString() + "): ") : ": ");
      do
      {
        p->Read(is);
      } while (!p->IsSet());
    }
  }

  /** Print all parameter values on an output stream. */
  virtual void Print(std::ostream &os = std::cout) const
  {
    for (auto p : this->parameters)
    {
      os << "  " << p->description << ": ";
      p->Write(os) << std::endl;
    }
  }

  /** Gets a given parameter */
  template <typename T>
  void GetParameterValue(std::string flag, T &value)
  {
    for (auto p : this->parameters)
    {
      if (p->cmdline_flag == flag || p->cmdline_flag == parameters.prefix + "::" + flag)
      {
        Parameter<T> *p_par = dynamic_cast<Parameter<T> *>(p);
        if (!p_par)
          throw std::logic_error("Parameter " + p->cmdline_flag + " value of an incorrect type");
        value = *p_par;
        return;
      }
    }
    // FIXME: a more specific exception should be raised
    throw std::logic_error("Parameter " + flag + " not in the list");
  }

  void CopyParameterValues(const Parametrized &p)
  {
    for (auto &p1 : this->parameters)
    {
      std::string f1 = split(p1->cmdline_flag, std::regex("::"))[1];
      for (const auto &p2 : p.parameters)
      {
        std::string f2 = split(p2->cmdline_flag, std::regex("::"))[1];
        if (f1 == f2)
        {
          p1->CopyValue(*p2);
          break;
        }
      }
    }
  }

  /** Sets a given parameter to a given value */
  template <typename T>
  void SetParameter(std::string flag, const T &value)
  {
    bool found = false;
    for (auto p : this->parameters)
    {
      if (p->cmdline_flag == flag || p->cmdline_flag == parameters.prefix + "::" + flag)
      {
        Parameter<T> *p_par = dynamic_cast<Parameter<T> *>(p);
        if (!p_par)
          throw std::logic_error("Parameter " + p->cmdline_flag + " value of an incorrect type");
        (*p_par) = value;
        found = true;
      }
    }
    // FIXME: a more specific exception should be raised
    if (!found)
      throw std::logic_error("Parameter " + flag + " not in the list");
  }

  bool IsRegistered() const
  {
    for (auto p : parameters)
      if (!p->IsValid())
        return false;
    return true;
  }

protected:
  void _RegisterParameters()
  {
    if (!parameters_registered)
    {
      InitializeParameters();
      parameters_registered = true;
    }
  }

  virtual void InitializeParameters() = 0;

  ParameterBox parameters;

  bool parameters_registered;

  ~Parametrized()
  {
    for (auto it = overall_parametrized.begin(); it != overall_parametrized.end(); ++it)
    {
      if (*it == this)
      {
        it = overall_parametrized.erase(it);
        break;
      }
    }
  }

  static std::list<Parametrized *> overall_parametrized;
};

} // namespace Core
} // namespace EasyLocal
