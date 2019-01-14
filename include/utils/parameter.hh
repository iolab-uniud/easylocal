#pragma once

#include <stdexcept>
#include <list>
#include <string>
#include <iostream>
#include <memory>
#include <sstream>
#include "utils/json.hpp"
#include "utils/types.hh"

#include "boost/program_options/options_description.hpp"
#include "boost/program_options/variables_map.hpp"
#include "boost/program_options/parsers.hpp"

namespace EasyLocal
{
  
  namespace Core
  {
    
    using json = nlohmann::json;
    
    /** Abstract parameter type, for containers. */
    class AbstractParameter
    {
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
      
      std::string GetDescription() const { return description; }
      
      std::string GetCmdlineFlag() const { return cmdline_flag; }
      
      /** To print out values. */
      virtual std::string ToString() const = 0;
      
      /** To transform into json. */
      virtual json ToJSON() const = 0;
      
      /** Get a json description. */
      virtual json JSONDescription() const = 0;
      
      /** Reads a value from a json. */
      virtual void FromJSON(json v) = 0;
      
      std::string Flag() const
      {
        return cmdline_flag;
      }
      
    protected:
      AbstractParameter()
      : is_set(false), is_valid(false)
      {}
      
      /** Can't instantiate an AbstractParameter from the outside. */
      AbstractParameter(const std::string &cmdline_flag, const std::string &description)
      : description(description), cmdline_flag(cmdline_flag), is_set(false), is_valid(true)
      {}
      
      /** Description of the parameter, for documentation and Parametrized::ReadParameters(), Parametrized::PrintParameters(). */
      std::string description;
      
      /** Flag representing the parameter. */
      std::string cmdline_flag;
      
      /** True if this is set. */
      bool is_set;
      
      /** True if it has been correctly instantiated. */
      bool is_valid;
      
    public:
      
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
    class ParameterBox : public std::vector<AbstractParameter*>
    {
    public:
      /** Constructor.
       @param prefix "namespace" of the parameter (e.g. specific solver or runner)
       @param description semantics of the parameter
       */
      ParameterBox(const std::string &prefix, const std::string &description)
      : prefix(prefix), cl_options(description)
      {
        OverallParameters().push_back(this);
      }
      
      void FromJSON(json parameters)
      {
        for (auto it = parameters.begin(); it != parameters.end(); ++it)
        {
          for (auto p : *this)
            if (p->Flag() == it.key() || p->Flag() == this->prefix + "::" + it.key())
              p->FromJSON({{ it.key(), it.value() }});
        }
      }
      json ToJSON() const
      {
        json parameters;
        parameters[this->prefix] = {};
        for (auto p : *this)
          parameters[this->prefix].merge_patch(p->ToJSON());
        return parameters;
      }
      
      json JSONDescription() const
      {
        json parameters;
        parameters[this->prefix] = {};
        for (auto p : *this)
          parameters[this->prefix].merge_patch(p->JSONDescription());
        return parameters;
      }
      
      /** Namespace of the parameter. */
      const std::string prefix;
      /** Object to configure boost's parameter parser. */
      boost::program_options::options_description cl_options;
      /** List of all parameter boxes that have been instantiated. */
      
      static std::list<const ParameterBox *>& OverallParameters()
      {
        static std::list<const ParameterBox*> overall_parameters;
        return overall_parameters;
      }
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
          throw ParameterNotValid(*this);
        std::stringstream ss;
        ss << this->value;
        return ss.str();
      }
      
      /** @copydoc AbstractParameter::ToJSON */
      virtual json ToJSON() const
      {
        if (!is_valid)
          throw ParameterNotValid(*this);
        json p;
        std::string flag = split(this->cmdline_flag, std::regex("::"))[1];
        p[flag] = this->value;
        
        return p;
      }
      
      /** @copydoc AbstractParameter::JSONDescription */
      virtual json JSONDescription() const
      {
        json p;
        std::string flag = split(this->cmdline_flag, std::regex("::"))[1];
        p[flag] = GetTypeName<T>();
        
        return p;
      }
      
      /** @copydoc AbstractParameter::FromJSON */
      virtual void FromJSON(json v)
      {
        std::string flag = split(this->cmdline_flag, std::regex("::"))[1];
        this->value = v[flag];
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
        parameters.cl_options.add_options()((flag + "-enable").c_str(), boost::program_options::value<std::string>()->implicit_value("true")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = true; }), "")((flag + "-disable").c_str(), boost::program_options::value<std::string>()->implicit_value("false")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = false; }),
                                                                                                                                                                                                                                                      ("[enable/disable] " + description).c_str());
      }
      
      virtual void operator()(const std::string &cmdline_flag, const std::string &description, ParameterBox &parameters)
      {
        this->cmdline_flag = parameters.prefix + "::" + cmdline_flag;
        this->description = description;
        parameters.push_back(this);
        parameters.cl_options.add_options()((this->cmdline_flag + "-enable").c_str(), boost::program_options::value<std::string>()->implicit_value("true")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = true; }), "")((this->cmdline_flag + "-disable").c_str(), boost::program_options::value<std::string>()->implicit_value("false")->zero_tokens()->notifier([this](const std::string &v) { this->is_set = true; this->value = false; }),
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
      IncorrectParameterValue(const Parameter<T> &p, std::string desc)
      : std::logic_error("Incorrect parameter value")
      {
        std::ostringstream os;
        os << "Parameter " << p.cmdline_flag << " set to incorrect value " << p.value << " (" << desc << ")";
        message = os.str();
      }
      virtual const char *what() const throw()
      {
          return message.c_str();
      }
      virtual ~IncorrectParameterValue()
      {}
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
        os << "NotValid";
        //throw ParameterNotValid(*this);
      else if (!is_set)
        //throw ParameterNotSet(*this);
        os << "NotSet";
      else
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
    
    class CommandLineParameters
    {
    public:
      /** A class representing a parametrized component of EasyLocal++. */
      class Parametrized
      {
        friend class CommandLineParameters;
        
      public:
        /** Constructor.
         @param prefix namespace of the parameters
         @param description semantics of the parameters group
         */
        Parametrized(const std::string &prefix, const std::string &description) : parameters(prefix, description), parameters_registered(false)
        {
          OverallParametrized().push_back(this);
        }
        
        static void RegisterParameters()
        {
          for (auto p : OverallParametrized())
            p->_RegisterParameters();
        }
        
        /** Read all parameters from an input stream (prints hints on output stream). */
        virtual void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout)
        {
          is.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
          
          for (auto p : this->parameters)
          {
            os << "  " << p->GetDescription() << (p->IsSet() ? (std::string(" (def.: ") + p->ToString() + "): ") : ": ");
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
            os << "  " << p->GetDescription() << ": ";
            p->Write(os) << std::endl;
          }
        }
        
        /** Gets a given parameter */
        template <typename T>
        void GetParameterValue(std::string flag, T &value)
        {
          for (auto p : this->parameters)
          {
            if (p->GetCmdlineFlag() == flag || p->GetCmdlineFlag() == parameters.prefix + "::" + flag)
            {
              Parameter<T> *p_par = dynamic_cast<Parameter<T> *>(p);
              if (!p_par)
                throw std::logic_error("Parameter " + p->GetCmdlineFlag() + " value of an incorrect type");
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
            std::string f1 = split(p1->GetCmdlineFlag(), std::regex("::"))[1];
            for (const auto &p2 : p.parameters)
            {
              std::string f2 = split(p2->GetCmdlineFlag(), std::regex("::"))[1];
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
            if (p->GetCmdlineFlag() == flag || p->GetCmdlineFlag() == parameters.prefix + "::" + flag)
            {
              Parameter<T> *p_par = dynamic_cast<Parameter<T> *>(p);
              if (!p_par)
                throw std::logic_error("Parameter " + p->GetCmdlineFlag() + " value of an incorrect type");
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
        
        json ParametersToJSON() const
        {
          return parameters.ToJSON();
        }
        
        json ParametersDescriptionToJSON() const
        {
          return parameters.JSONDescription();
        }
        
        void ParametersFromJSON(json p)
        {
          parameters.FromJSON(std::move(p));
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
          std::list<Parametrized*> overall_parametrized = OverallParametrized();
          for (auto it = overall_parametrized.begin(); it != overall_parametrized.end(); ++it)
          {
            if (*it == this)
            {
              it = overall_parametrized.erase(it);
              break;
            }
          }
        }
        
        static std::list<Parametrized*>& OverallParametrized()
        {
          static std::list<Parametrized*> overall_parametrized;
          return overall_parametrized;
        }
      };
      
      
      static bool Parse(int argc, const char *argv[], bool check_unregistered = true, bool silent = false)
      {
        Parametrized::RegisterParameters();
        
        boost::program_options::options_description cmdline_options(argv[0]);
        boost::program_options::variables_map vm;
        for (auto pb : ParameterBox::OverallParameters())
          cmdline_options.add(pb->cl_options);
        
        cmdline_options.add_options()("help", "Produce help message");
        
        boost::program_options::parsed_options parsed = boost::program_options::command_line_parser(argc, argv).options(cmdline_options).allow_unregistered().run();
        std::vector<std::string> unrecognized_options = boost::program_options::collect_unrecognized(parsed.options, boost::program_options::include_positional);
        
        if (check_unregistered && unrecognized_options.size() > 0)
        {
          std::cout << "Unrecognized options: ";
          for (const std::string &o : unrecognized_options)
            std::cout << o << " ";
          std::cout << std::endl
          << "Run " << argv[0] << " --help for the allowed options" << std::endl;
          return false;
        }
        
        boost::program_options::store(parsed, vm);
        boost::program_options::notify(vm);
        
        if (!silent && vm.count("help"))
        {
          std::cout << cmdline_options << std::endl;
          return false;
        }
        return true;
      }
    };
    
    
    inline bool operator==(const Parameter<std::string> &s1, const char *s2)
    {
      return static_cast<std::string>(s1) == std::string(s2);
    }    
  } // namespace Core
} // namespace EasyLocal
