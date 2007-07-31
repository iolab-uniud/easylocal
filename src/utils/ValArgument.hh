/** A class for valued arguments of the command line. */

#ifndef _VALARGUMENT_HH
#define _VALARGUMENT_HH

#include "Argument.hh"
#include <sstream>

template <typename T, unsigned int N = 1>
class ValArgument : public Argument
{
public:
  ValArgument(const std::string& flag, bool required)
    : Argument(flag, required) { values.resize(N); }
  ValArgument(const std::string& flag, const T& def_value, bool required)
    : Argument(flag, required), value(def_value), values(def_value, N) { }
  const T& getValue() const { return value; }
  const T& getValue(unsigned i) const { return values[i]; }
  const std::vector<T>& getValues() const { return values; }
  unsigned int numOfValues() const { return N; }
  void read(const std::string& val);
	void read(const std::vector<std::string>& val);
	void printUsage(std::ostream& os, unsigned int tabs = 1) const;
protected:
  T value;
  std::vector<T> values;
};

template <typename T, unsigned int N>
void ValArgument<T, N>::read(const std::string& val)
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
void ValArgument<T, N>::read(const std::vector<std::string>& val)
{
	bool fail = false;
	if (val.size() < N)
		throw new std::runtime_error("Not enough values for ValArgument");
	
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
void ValArgument<T, N>::printUsage(std::ostream& os, unsigned int tabs) const
{
	Argument::printUsage(os, tabs);
	if (N == 1)
			os << " <value>";
	else
		for (unsigned int i = 1; i <= N; i++)
			os << " <value" << i << ">";
}	

#endif

