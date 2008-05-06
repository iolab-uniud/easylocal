/* A command line parser class */

#ifndef _CLPARSER_HH
#define _CLPARSER_HH

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
#include <list>
#include <sstream>

class CLParser;

class Argument 
{
public:
  virtual const std::string& GetFlag() const { return flag; }
  virtual const std::string& GetAlias() const { return alias; }
  void SetAlias(const std::string& s) { alias = s; }
  virtual void Read(const std::string& val) = 0;
  virtual void Read(const std::vector<std::string>& val) = 0;
  virtual void PrintUsage(std::ostream& os, unsigned int tabs = 1) const;
  virtual unsigned int NumOfValues() const = 0;
  bool IsSet() const { return value_set; }
  bool IsRequired() const { return required; }
protected:
  Argument(const std::string& fl, const std::string& al, bool req);
  Argument(const std::string& fl, const std::string& al, bool req, CLParser& cl);
  virtual ~Argument() {}
  std::string flag, alias;
  bool value_set;
  bool required;
};

class ArgumentGroup : public Argument
{
  friend class CLParser;
public:
  ArgumentGroup(const std::string& flag, const std::string& alias, bool required);
  ArgumentGroup(const std::string& flag, const std::string& alias, bool required, CLParser& cl);
  void Read(const std::string& val);
  void Read(const std::vector<std::string>& val);
  void AddArgument(Argument& a);
  unsigned int NumOfValues() const
  { return num_of_values; }
  void PrintUsage(std::ostream& os, unsigned int tabs = 0) const;
protected:
  typedef std::list<Argument*> arg_list;
  ArgumentGroup(arg_list& al);
  Argument& FindArgument(const std::string&) const throw (std::logic_error);
  arg_list arguments;
  unsigned int num_of_values;
};



template <typename T, unsigned int N = 1>
class ValArgument : public Argument
{
public:
  ValArgument(const std::string& flag, const std::string& alias, bool required)
    : Argument(flag, alias, required) { values.resize(N); }
  ValArgument(const std::string& flag, const std::string& alias, bool required, T def_value)
    : Argument(flag, alias, required), value(def_value), values(N, def_value) { }
  ValArgument(const std::string& flag, const std::string& alias, bool required, CLParser& cl)
    : Argument(flag, alias, required, cl) { values.resize(N); }
  ValArgument(const std::string& flag, const std::string& alias, bool required, T def_value, CLParser& cl)
    : Argument(flag, alias, required, cl), value(def_value), values(N, def_value) {}
  const T& GetValue() const { return value; }
  const T& GetValue(unsigned i) const { if (N == 1) return value; else return values[i]; }
  const std::vector<T>& GetValues() const { return values; }
  unsigned int NumOfValues() const { return N; }
  void Read(const std::string& val);
  void Read(const std::vector<std::string>& val);
  void PrintUsage(std::ostream& os, unsigned int tabs = 1) const;
protected:
  T value;
  std::vector<T> values;
};

template <typename T, unsigned int N>
void ValArgument<T, N>::Read(const std::string& val)
{
  std::istringstream is(val);
  if (N == 1)
    is >> value;
  else
    for (unsigned int i = 0; i < values.size(); i++)
      is >> values[i];
  value_set = !is.fail();
}

template <typename T, unsigned int N>
void ValArgument<T, N>::Read(const std::vector<std::string>& val)
{
  bool fail = false;
  if (val.size() < N)
    throw new std::logic_error("Not enough values for ValArgument");
	
  if (N == 1) 
    {
      std::istringstream is(val[0]);
      is >> value;
      fail = is.fail();
    } 
  else
    for (unsigned int i = 0; i < values.size(); i++) 
      {
	std::istringstream is(val[i]);
	is >> values[i];
	fail = is.fail();
      }
  value_set = !fail;
}

template <typename T, unsigned int N>
void ValArgument<T, N>::PrintUsage(std::ostream& os, unsigned int tabs) const
{
  Argument::PrintUsage(os, tabs);
  if (N == 1)
    os << " <value>";
  else
    for (unsigned int i = 1; i <= N; i++)
      os << " <value" << i << ">";
}	

//class ArgumentGroup;

class FlagArgument : public Argument
{
public:
  FlagArgument(const std::string& flag, const std::string& alias, bool required)
    : Argument(flag, alias, required) {}
  FlagArgument(const std::string& flag, const std::string& alias, bool required, CLParser& cl)
    : Argument(flag, alias, required, cl) {}
  void Read(const std::string& val)
  { value_set = true; }
  void Read(const std::vector<std::string>& val)
  { value_set = true; }
  unsigned int NumOfValues() const
  { return 0; }
};

class CLParser // : public CLParserInterface
{
  friend std::ostream& operator<<(std::ostream& os, const CLParser& cl);
public:
  CLParser(int argc, char * const argv[]);
  void AddArgument(Argument& a);
  void MatchArguments();
  void MatchArgument(Argument& a);
  virtual ~CLParser();
protected:
  typedef std::list<Argument*> arg_list;
  void Parse() throw (std::logic_error);
  std::string command_name;
  std::vector<std::string> command_line_arguments;
  arg_list arguments;
};

std::ostream& operator<<(std::ostream& os, const CLParser& cl);

//extern CLParser nullCL;

#endif
