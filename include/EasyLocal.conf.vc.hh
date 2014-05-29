
// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.
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