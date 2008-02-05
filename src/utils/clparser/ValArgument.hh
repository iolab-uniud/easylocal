/** A class for valued arguments of the command line. */

#ifndef _VALARGUMENT_HH
#define _VALARGUMENT_HH

#include "Argument.hh"
#include <sstream>


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

#endif

