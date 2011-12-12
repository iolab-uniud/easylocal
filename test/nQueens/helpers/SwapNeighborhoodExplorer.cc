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

#include "SwapNeighborhoodExplorer.hh"
#include <algorithm>
#include <utils/Random.hh>

// the first parameter is not used, therefore it is not named
// (CC gives a warning if it is named)
void SwapNeighborhoodExplorer::RandomMove(const std::vector<int> &, Swap& sw) const
{
  sw.from = Random::Int(0, in - 1);
  do
    sw.to = Random::Int(0, in - 1);
  while (sw.from == sw.to);
  if (sw.from > sw.to) // swap from and to so that from < to
    std::swap(sw.from, sw.to);
}

bool SwapNeighborhoodExplorer::NextMove(const std::vector<int> &, Swap& sw) const
{
  if (sw.to < in - 1) 
  {
    sw.to++;
    return true;
  }
  else if (sw.from < in - 2)
  { 
    sw.from++; 
    sw.to = sw.from + 1; 
    return true;
  }
  else
    return false;
}


void SwapNeighborhoodExplorer::FirstMove(const std::vector<int> &, Swap& sw) const
{
  sw.from = 0; 
  sw.to = 1; 
}



void SwapNeighborhoodExplorer::MakeMove(std::vector<int> &a, const Swap& sw) const
{ 
	std::swap(a[sw.from], a[sw.to]);
}
