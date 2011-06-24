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

#if !defined(_SWAPNEIGHBORHOODEXPLORER_HH_)
#define _SWAPNEIGHBORHOODEXPLORER_HH_

#include <helpers/NeighborhoodExplorer.hh>
#include "../data/Swap.hh"
#include "QueensStateManager.hh"

/** This class implements a possible neighborhood explorartion strategy
for the Swap move. */
class SwapNeighborhoodExplorer
:  public NeighborhoodExplorer<int,std::vector<int>,Swap>
{
public:
	SwapNeighborhoodExplorer(const int& in, StateManager<int, std::vector<int> >& qsm, std::string name = "")
	: NeighborhoodExplorer<int,std::vector<int>,Swap>(in, qsm, name)
  {}
	void MakeMove(std::vector<int> &a, const Swap& sw) const;
  void RandomMove(const std::vector<int> &a, Swap& sw) const;
  void FirstMove(const std::vector<int> &a, Swap& sw) const;
  bool NextMove(const std::vector<int> &a, Swap& sw) const;
};

#endif /*_SWAPNEIGHBORHOODEXPLORER_HH_*/
