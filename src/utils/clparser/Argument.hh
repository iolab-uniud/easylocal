/** An abstract class for command line arguments . */

#ifndef _ARGUMENT_HH
#define _ARGUMENT_HH

#include <string>
#include <vector>
#include <iostream>

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

#endif

