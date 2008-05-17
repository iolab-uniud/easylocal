/**
 @file EasyLocal.conf.vc.hh
 @brief Basic configuration for Visual C++.
 
 This file contains the configuration of the framework.   
 
 @author Andrea Schaerf (schaerf@uniud.it), Luca Di Gaspero (l.digaspero@uniud.it)
 @version 1.0
 $Revision$
 @date 15 Jun 2001
 @note This version works both with MS Visual C++ and the GNU C++ 
 compiler. Yet, it is extensively tested only with the GNU compiler.
*/

#if !defined(_EASYLOCAL_CONF_VC_HH_)
#define _EASYLOCAL_CONF_VC_HH_

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


#endif
