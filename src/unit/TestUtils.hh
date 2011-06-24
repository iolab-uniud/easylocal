// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2011 Andrea Schaerf, Luca Di Gaspero. 
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

#if !defined(_TESTUTILS_HH_)
#define _TESTUTILS_HH_

#include <string>
#include <sstream>

// For letting unit tests access also to private and protected members
#define protected public
#define private public

inline std::string stringify(const char* message, const char* file, unsigned int line)
{
	std::ostringstream s;
	s << message << " at " << file << ":" << line;
	
	return s.str();
}

#endif //_TESTUTILS_HH_

