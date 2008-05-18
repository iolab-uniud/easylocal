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

#include "Swap.hh"

Swap::Swap(int f, int t)
{
    from = f;
    to = t;
}

bool Swap::operator==(const Swap& m) const
{
    return (from == m.from) && (to == m.to);
}

bool Swap::operator!=(const Swap& m) const
{
    return (from != m.from) || (to != m.to);
}

bool Swap::operator<(const Swap& m) const
{
    return from < m.from || (from == m.from && to < m.to);
}

std::ostream& operator<<(std::ostream& os, const Swap& m)
{
    return os << '(' << m.from << ',' << m.to << ')';
}

std::istream& operator>>(std::istream& is, Swap& m)
{
    return is >>  m.from >> m.to;
}
