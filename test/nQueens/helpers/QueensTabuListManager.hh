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

#if !defined(_QUEENSTABULISTMANAGER_HH_)
#define _QUEENSTABULISTMANAGER_HH_

#include <helpers/TabuListManager.hh>
#include <vector>
#include "../data/Swap.hh"

/** This class handles the tabu list. */
class QueensTabuListManager : public TabuListManager<std::vector<int>, Swap>
{
/* public:
	QueensTabuListManager() 
	: TabuListManager<std::vector<int>, Swap>() {} */
protected:
	bool Inverse(const Swap& m1, const Swap& m2) const { return m1 == m2; }
};

class QueensFrequencyTabuListManager : public FrequencyTabuListManager<std::vector<int>, Swap>
{
public:
	QueensFrequencyTabuListManager(int min = 0, int max = 0) 
	: FrequencyTabuListManager<std::vector<int>, Swap>(min,max) {}
protected:
	bool Inverse(const Swap& m1, const Swap& m2) const; 
};

bool QueensFrequencyTabuListManager::Inverse(const Swap& m1, const Swap& m2) const
{ return m1 == m2; }


#endif /*_QUEENSTABULISTMANAGER_HH_*/
