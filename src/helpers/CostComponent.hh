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

#if !defined(_COST_COMPONENT_HH_)
#define _COST_COMPONENT_HH_

#include <iostream>

/** 
 @brief The class CostComponent manages one single component of the
 cost, either hard or soft.
 
 @ingroup Helpers      
*/

template <class Input, class State, typename CFtype = int>
class CostComponent
{
public:
  void Print(std::ostream& os = std::cout) const;
  virtual CFtype ComputeCost(const State& st) const = 0;
  CFtype Cost(const State& st) const { return weight * ComputeCost(st); }
  virtual void PrintViolations(const State& st, std::ostream& os = std::cout) const = 0;
  CFtype Weight() const { return weight; }
  void SetWeight(const CFtype& w) { weight = w; }
  void SetHard() { is_hard = true; }
  void SetSoft() { is_hard = false; }
  bool IsHard() const { return is_hard; }
  bool IsSoft() const { return !is_hard; }
  const std::string name;
protected:
  CostComponent(const Input& in, const CFtype& weight, bool hard, std::string name);
  virtual ~CostComponent() {}
  const Input& in;
  CFtype weight;
  bool is_hard;
};

/*************************************************************************
 * Implementation
 *************************************************************************/
 
template <class Input, class State, typename CFtype>
CostComponent<Input,State,CFtype>::CostComponent(const Input& i, const CFtype& w, bool hard, std::string e_name)
  : name(e_name), in(i), weight(w), is_hard(hard)
{}

template <class Input, class State, typename CFtype>
void CostComponent<Input,State,CFtype>::Print(std::ostream& os) const
{ os  << "Cost Component " << name << ": weight " << weight << (is_hard ? "*" : "") << std::endl; }

#endif
