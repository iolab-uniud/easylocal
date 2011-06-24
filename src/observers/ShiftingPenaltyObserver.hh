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

#if !defined(_SHIFTING_PENALTY_OBSERVER_HH_)
#define _SHIFTING_PENALTY_OBSERVER_HH_

template <typename CFtype>
class ShiftingPenaltyManager;

template <typename CFtype = int>
class ShiftingPenaltyObserver
{
public:
  ShiftingPenaltyObserver(std::ostream& r_os = std::cout);
  void NotifyReset(ShiftingPenaltyManager<CFtype>& r);
  void NotifyUpdate(ShiftingPenaltyManager<CFtype>& r, CFtype cost);
  void NotifyNewThreshold(ShiftingPenaltyManager<CFtype>& r);
protected:
  std::ostream& os;
};

template <typename CFtype>
ShiftingPenaltyObserver<CFtype>::ShiftingPenaltyObserver(std::ostream& r_os) 
  : os(r_os) {}

template <typename CFtype>
void ShiftingPenaltyObserver<CFtype>::NotifyReset(ShiftingPenaltyManager<CFtype>& r)
{
  os << "Reset: " << r.name << " " << r.shift << " " << (CFtype)0 << std::endl;
}

template <typename CFtype>
void ShiftingPenaltyObserver<CFtype>::NotifyUpdate(ShiftingPenaltyManager<CFtype>& r, CFtype cost)
{
  os << "Update: " << r.name << " " << r.shift << " " << cost << std::endl;
}

template <typename CFtype>
void  ShiftingPenaltyObserver<CFtype>::NotifyNewThreshold(ShiftingPenaltyManager<CFtype>& r)
{
  os << "NewThreshold: " << r.name << " " << r.cost_threshold << std::endl;
}


#endif /*OBSERVER_HH_*/
