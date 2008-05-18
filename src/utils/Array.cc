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

#include <utils/Array.hh>

/**
  Index utilities
 */

std::set<unsigned int> seq(unsigned int s, unsigned int e)
{
	std::set<unsigned int> tmp;
	for (unsigned int i = s; i <= e; i++)
		tmp.insert(i);
	
	return tmp;
}

std::set<unsigned int> singleton(unsigned int i)
{
	std::set<unsigned int> tmp;
	tmp.insert(i);
	
	return tmp;
}
