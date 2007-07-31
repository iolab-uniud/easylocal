/** An abstract class for command line arguments . */

#ifndef _ARGUMENT_HH
#define _ARGUMENT_HH

#include <string>
#include <vector>
#include <iostream>

class Argument 
{
public:
  virtual const std::string& getFlag() const { return flag; }
  virtual const std::string& getAlias() const { return alias; }
  void setAlias(const std::string& s) { alias = s; }
  virtual void read(const std::string& val) = 0;
	virtual void read(const std::vector<std::string>& val) = 0;
	virtual void printUsage(std::ostream& os, unsigned int tabs = 1) const;
  virtual unsigned int numOfValues() const = 0;
  bool isSet() const { return value_set; }
  bool isRequired() const { return required; }
protected:
  Argument(const std::string& fl, bool req) 
    : flag(fl), alias(""), value_set(false), required(req)
  { }
  virtual ~Argument() {}
  std::string flag, alias;
  bool value_set;
  bool required;
};

#endif

