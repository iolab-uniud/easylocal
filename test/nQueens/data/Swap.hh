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

#if !defined(_SWAP_H_)
#define _SWAP_H_

#include <iostream>

/** This class instantiates template Move. It represents a swap of the
row position between two queens.
@ingroup basic */
class Swap
{
public:
    Swap(int f = 0, int t = 0);
    bool operator==(const Swap&) const;
    bool operator!=(const Swap&) const;
    bool operator<(const Swap&) const;
    int from, to;
};

std::istream& operator>>(std::istream& is, Swap& m);
std::ostream& operator<<(std::ostream& os, const Swap& m);

#endif /*_SWAP_H_*/
