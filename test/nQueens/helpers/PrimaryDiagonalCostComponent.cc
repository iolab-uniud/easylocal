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

#include "PrimaryDiagonalCostComponent.hh"

bool PrimaryDiagonalCostComponent::Violation(int i, int j, int ai, int aj) 
{
	if (i != j)
		return (j - i == aj - ai);
	else
		return false;
}

int PrimaryDiagonalCostComponent::ComputeCost(const std::vector<int>& a) const
{
  int violations = 0;
  for (int i = 0; i < in - 1; i++)
    for (int j = i + 1; j < in; j++)
      if (Violation(i, j, a[i], a[j]))
				violations++;
  return violations;
}

void PrimaryDiagonalCostComponent::PrintViolations(const std::vector<int>& a, std::ostream& os) const
{
  for (int i = 0; i < in - 1; i++)
    for (int j = i + 1; j < in; j++)
      if (Violation(i, j, a[i], a[j]))
				os << "Queens " << i << " and " << j << " are in primary diagonal conflict" << std::endl;	
}

