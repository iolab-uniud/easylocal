#if !defined(_TYPES_HH_)
#define _TYPES_HH_

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif
#include <vector>

template <typename CFtype>
bool IsZero(CFtype value);

template <typename CFtype>
bool EqualTo(CFtype value1, CFtype value2);

template <typename CFtype>
bool LessThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool LessOrEqualThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool GreaterThan(CFtype value1, CFtype value2);

template <typename CFtype>
bool GreaterOrEqualThan(CFtype value1, CFtype value2);

template <typename CFtype>
CFtype max(const std::vector<CFtype>& values)
{
 CFtype max_val = values[0];
 for (unsigned int i = 1; i < values.size(); i++)
 if (values[i] > max_val)
 max_val = values[i];
 
 return max_val;
}
 
template <typename CFtype>
CFtype min(const std::vector<CFtype>& values)
{
 CFtype min_val = values[0];
 for (unsigned int i = 1; i < values.size(); i++)
 if (values[i] < min_val)
 min_val = values[i];
 
 return min_val;
}

#endif // _TYPES_HH_
