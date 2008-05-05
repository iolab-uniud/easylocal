#if !defined(_TYPES_HH_)
#define _TYPES_HH_

#include <EasyLocal.conf.hh>
#if defined(HAVE_CONFIG_H)
#include <config.hh>
#endif

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

#if defined(_MSC_VER)
// in Microsoft Visual C++ the I/O operators on strings are not defined.
#include <iostream>
#include <string>

inline std::ostream& operator<<(std::ostream& os, const std::string& s)
{ 
	os << s.c_str(); 
	return os;
}

inline std::istream& operator>>(std::istream& is, std::string& s)
{ 
	char c[2048]; 
	is >> c; s = c; 
	return is;
}
#endif 

#endif // !defined(_TYPES_HH_)
