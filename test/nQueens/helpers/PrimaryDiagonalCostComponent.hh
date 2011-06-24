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

#if !defined(_PRIMARYDIAGONALCOSTCOMPONENT_HH_)
#define _PRIMARYDIAGONALCOSTCOMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <vector>

  class PrimaryDiagonalCostComponent : public CostComponent<int, std::vector<int> >
  {
  public:
    PrimaryDiagonalCostComponent(const int& in) : CostComponent<int, std::vector<int> >(in, 1, false, "PrimaryDiagonal") 
      {  }
    int ComputeCost(const std::vector<unsigned>& st) const;
    void PrintViolations(const std::vector<unsigned>& st, std::ostream& os = std::cout) const;
    int ComputeCost(const std::vector<int>& st) const;
		static bool Violation(int i, int j, int ai, int aj);
    void PrintViolations(const std::vector<int>& st, std::ostream& os = std::cout) const;
  };
  
  

#endif /*_PRIMARYDIAGONALCOSTCOMPONENT_HH_*/
