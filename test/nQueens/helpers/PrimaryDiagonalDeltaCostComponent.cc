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

#include "PrimaryDiagonalDeltaCostComponent.hh"

int PrimaryDiagonalDeltaCostComponent::ComputeDeltaCost(const std::vector<int>& a, const Swap& sw) const {
  int violations = 0;
  for (int i = 0; i < in; i++)
	{
		if (i == sw.from || i == sw.to)
			continue;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.from, a[i], a[sw.from]))
			violations--;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.to, a[i], a[sw.to]))
			violations--;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.from, a[i], a[sw.to]))
			violations++;
		if (PrimaryDiagonalCostComponent::Violation(i, sw.to, a[i], a[sw.from]))
			violations++;		
	}
	if (PrimaryDiagonalCostComponent::Violation(sw.from, sw.to, a[sw.from], a[sw.to]))
		violations--;
	if (PrimaryDiagonalCostComponent::Violation(sw.from, sw.to, a[sw.to], a[sw.from]))
		violations++;
  return violations;
}
