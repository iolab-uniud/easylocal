#include "boost/program_options/parsers.hpp"
#include "boost/program_options/variables_map.hpp"

#include "easylocal/utils/parameter.hh"

using namespace EasyLocal::Core;

// Overall collection of aprameters
std::list<const ParameterBox*> ParameterBox::overall_parameters;
std::list<Parametrized*> Parametrized::overall_parametrized;

IncorrectParameterValue::~IncorrectParameterValue() throw() {}

const char* IncorrectParameterValue::what() const throw()
{
  return message.c_str();
}

AbstractParameter::AbstractParameter()
: is_set(false), is_valid(false)
{}

AbstractParameter::AbstractParameter(const std::string& cf, const std::string& d)
: description(d), cmdline_flag(cf), is_set(false), is_valid(true)
{}

ParameterBox::ParameterBox(const std::string& p, const std::string& description) : prefix(p), cl_options(description)
{
  overall_parameters.push_back(this);
}

namespace EasyLocal
{
  namespace Core
  {
    // Specialization for bool parameters (handle as enable/disable flags)
    template <>
    Parameter<bool>::Parameter(const std::string& cmdline_flag, const std::string& description, ParameterBox& parameters)
    : AbstractParameter(cmdline_flag, description)
    {
      std::string flag = parameters.prefix + "::" + cmdline_flag;
      parameters.cl_options.add_options()
      (("enable-" + flag).c_str(), boost::program_options::value<std::string>()->implicit_value("true")->zero_tokens()->notifier([this](const std::string& v){ this->is_set = true; this->value = true; }), "")
      (("disable-" + flag).c_str(), boost::program_options::value<std::string>()->implicit_value("false")->zero_tokens()->notifier([this](const std::string & v){ this->is_set = true; this->value = false; }),
       ("[enable/disable] " + description).c_str());
    }
  }  
}

// Parameter parsing (courtesy of Boost)
bool CommandLineParameters::Parse(int argc, const char* argv[], bool check_unregistered, bool silent)
{
  for (auto pz : Parametrized::overall_parametrized)
    pz->RegisterParameters();
  
  boost::program_options::options_description cmdline_options(argv[0]);
  boost::program_options::variables_map vm;
  for (auto pb : ParameterBox::overall_parameters)
    cmdline_options.add(pb->cl_options);
  
  cmdline_options.add_options()("help", "Produce help message");
  
  boost::program_options::parsed_options parsed = boost::program_options::command_line_parser(argc, argv).options(cmdline_options).allow_unregistered().run();
  std::vector<std::string> unrecognized_options = boost::program_options::collect_unrecognized(parsed.options, boost::program_options::include_positional);
  
  if (check_unregistered && unrecognized_options.size() > 0)
  {
    std::cout << "Unrecognized options: ";
    for (const std::string& o : unrecognized_options)
      std::cout << o << " ";
    std::cout << std::endl << "Run " << argv[0] << " --help for the allowed options" << std::endl;
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

bool operator==(const Parameter<std::string>& s1, const char* s2) throw (ParameterNotSet)
{
  return static_cast<std::string>(s1) == std::string(s2);
}
