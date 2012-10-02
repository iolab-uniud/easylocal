#include "Parameter.hh"
#if defined(HAVE_LINKABLE_BOOST)
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#endif

IncorrectParameterValue::~IncorrectParameterValue() throw() { }

AbstractParameter::AbstractParameter(const std::string& cf, const std::string& d)
: description(d), cmdline_flag(cf)
{}

std::vector<const ParameterBox*> ParameterBox::overall_parameters;

ParameterBox::ParameterBox(const std::string& p, const std::string& description) : prefix(p)
#if defined(HAVE_LINKABLE_BOOST)
, cl_options(description)
#endif
{
  overall_parameters.push_back(this);
}

const char* IncorrectParameterValue::what() const throw()
{
  return message.c_str();
}

// specialization for bool parameters (i.e., enable/disable flags)
template <>
Parameter<bool>::Parameter(const std::string& cmdline_flag, const std::string& description, ParameterBox& parameters)
: AbstractParameter(cmdline_flag, description), is_set(false)
{
  std::string flag = parameters.prefix + "::" + cmdline_flag;
#if defined(HAVE_LINKABLE_BOOST)
  parameters.cl_options.add_options()
  (("enable-" + flag).c_str(), boost::program_options::value<std::string>()->implicit_value("true")->zero_tokens()->notifier([this](const std::string& v){ this->is_set = true; this->value = true; }), "")
  (("disable-" + flag).c_str(), boost::program_options::value<std::string>()->implicit_value("false")->zero_tokens()->notifier([this](const std::string & v){ this->is_set = true; this->value = false; }), ("[enable/disable] " + description).c_str());
#endif
}

bool CommandLineParameters::Parse(int argc, const char* argv[], bool check_unregistered)
{
#if defined(HAVE_LINKABLE_BOOST)
  static bool help_already_added = false;
  boost::program_options::options_description cmdline_options;
  boost::program_options::variables_map vm;
  for (auto pb : ParameterBox::overall_parameters)
    cmdline_options.add(pb->cl_options);
  if (!help_already_added)
  {
    cmdline_options.add_options()
    ("help", "Produce help message");
    help_already_added = true;
  }
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
  if (vm.count("help"))
  {
    std::cout << cmdline_options << std::endl;
    return false;
  }
#endif
  return true;
}
