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

#include "QueensStateManager.hh"
#include <utils/Random.hh>
#include <vector>
#include <stdexcept>

QueensStateManager::QueensStateManager(const int& bs)
: StateManager<int,std::vector<int> >(bs, "QueensStateManager")
{}

void QueensStateManager::RandomState(std::vector<int> &a)
{
	std::vector<bool> tag(in, false);
	int i, j;
	
	for (j = 0; j < in; j++)
	{ 
		do
			i = Random::Int(0, in - 1);
		while (tag[i]);
		tag[i] = true;
		a[j] = i;
	}
}

bool QueensStateManager::CheckConsistency(const std::vector<int> &a) const
{
  std::vector<bool> tag(in, false);
  for (int j = 0; j < in; j++)
    {
      if (a[j] >= in)
	throw std::runtime_error("State is not consistent (queen out of the chessboard)");
      if (tag[a[j]])
	throw std::runtime_error("State is not consistent (queens do not form a permutation)");
      tag[a[j]] = true;
    }
  return true;
}

std::ostream& operator<<(std::ostream& os, const std::vector<int>& a)
{
	for (int i = 0; i < (int)a.size(); i++)
		os << a[i] << ' ';
	os << std::endl;
	
	return os;
}
