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

#if !defined(_QUEENSKICKER_HH_)
#define _QUEENSKICKER_HH_

#include <kickers/SimpleKicker.hh>
#include <vector>
#include "../data/Swap.hh"

class QueensKicker : public SimpleKicker<int,std::vector<int>,Swap>
{
public:
	QueensKicker(const int& bs, SwapNeighborhoodExplorer& qnhe, int s = 2)
	: SimpleKicker<int,std::vector<int>,Swap>(bs, qnhe, s, "QueensKicker") 
{}
	bool RelatedMoves(const Swap&, const Swap&) const
{ return true; } 
};  	

#endif /*_QUEENSKICKER_HH_*/
