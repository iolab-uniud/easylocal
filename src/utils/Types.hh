#ifndef TYPES_HH_
#define TYPES_HH_

#ifdef _HAVE_EASYLOCALCONFIG
#include <EasyLocalConfig.hh>
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

#ifdef _MSC_VER
#include <iostream>
#include <string>

inline std::ostream& operator<<(std::ostream& os, const std::string& s)
{ 
	os << s.c_str(); 
	return os;
}

inline std::istream& operator<<(std::istream& is, std::string& s)
{ 
	char c[2048]; 
	is >> c; s = c; 
	return is;
}
#endif 

#endif /*TYPES_HH_*/
