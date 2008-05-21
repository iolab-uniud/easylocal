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

#if !defined(_SHIFTING_PENALTY_MANAGER_HH_)
#define _SHIFTING_PENALTY_MANAGER_HH_

#include <helpers/DeltaCostComponent.hh>
#include <helpers/CostComponent.hh>
#include <utils/Random.hh>
#include <observers/ShiftingPenaltyObserver.hh>

/** This class implements the shifting penalty mechanism for a single
    DeltaCost component */
template <typename CFtype = int>
class ShiftingPenaltyManager
{
  friend class ShiftingPenaltyObserver<CFtype>;
public:
  ShiftingPenaltyManager(CFtype threshold = 0, double s = 1.0, std::string n = "ShiftingPenaltyManager");
  virtual ~ShiftingPenaltyManager() {}
  void AttachObserver(ShiftingPenaltyObserver<CFtype>& ob) { observer = &ob; }

  void Print(std::ostream& os = std::cout) const;
  virtual void ReadParameters(std::istream& is = std::cin,
 		      std::ostream& os = std::cout) = 0;
  virtual bool Reset() = 0;
  virtual bool Update(CFtype cost) = 0;
  double Shift() const { return shift; }
  void SetShiftRange(double s1, double s2) { min_shift = s1; max_shift = s2; }
  void SetStartShift(double s) { start_shift = s; shift = s; }
  void SetCostThreshold(CFtype t) { cost_threshold = t; }
  double Threshold() const { return cost_threshold; }
  void SetPerturbRange(double min_p, double max_p) { min_perturb = min_p; max_perturb = max_p; }
  void SetPerturbValue(double p) { min_perturb = p - (p-1)/10; max_perturb = p + (p-1)/10; }
protected:
  double min_shift;
  double max_shift;
  double cost_threshold;
  std::string name;
  double min_perturb, max_perturb;
  double start_shift, shift;
  ShiftingPenaltyObserver<CFtype>* observer;
};


template <typename CFtype = int>
class ComplexShiftingPenaltyManager : public ShiftingPenaltyManager<CFtype>
{
public:
  ComplexShiftingPenaltyManager(CFtype threshold = 0, double s = 1.0, std::string n = "ComplexShiftingPenaltyManager");
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  bool Reset();
  bool Update(CFtype cost);
  void SetMaxFeasibleIterations(unsigned mf) { max_feasible_iter = mf; }
  void SetMaxInfeasibleIterations(unsigned mf) { max_infeasible_iter = mf; }
protected:
  unsigned int max_feasible_iter;
  unsigned int max_infeasible_iter;
  unsigned int feasible_iter;
  unsigned int infeasible_iter;
};

template <typename CFtype = int>
class SimpleShiftingPenaltyManager : public ShiftingPenaltyManager<CFtype>
{
public:
  SimpleShiftingPenaltyManager(double min_perturb, double max_perturb, CFtype threshold, double s, std::string n = "Shifting Penalty Manager");
  SimpleShiftingPenaltyManager(CFtype threshold = 0, double s = 1.0, std::string n = "SimpleShiftingPenaltyManager");
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  bool Reset();
  bool Update(CFtype cost);
protected:
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <typename CFtype>
ShiftingPenaltyManager<CFtype>::ShiftingPenaltyManager(CFtype threshold, double s, std::string n)
  : min_shift(0.0001), max_shift(1.0), cost_threshold(threshold), name(n), min_perturb(1.03), max_perturb(1.05),
    start_shift(s), shift(s), observer(NULL)  {}

template <typename CFtype>
 ComplexShiftingPenaltyManager<CFtype>::ComplexShiftingPenaltyManager(CFtype threshold, double s, std::string n)
   : ShiftingPenaltyManager<CFtype>(threshold,s,n),
     max_feasible_iter(10), max_infeasible_iter(10),
     feasible_iter(0), infeasible_iter(0) {}

template <typename CFtype>
SimpleShiftingPenaltyManager<CFtype>::SimpleShiftingPenaltyManager(double min_p, double max_p, CFtype threshold, double s, std::string n)
  : ShiftingPenaltyManager<CFtype>(threshold,s,n, min_p, max_p) {}

template <typename CFtype>
SimpleShiftingPenaltyManager<CFtype>::SimpleShiftingPenaltyManager(CFtype threshold, double s, std::string n)
  : ShiftingPenaltyManager<CFtype>(threshold,s,n) {}


template <typename CFtype>
void ShiftingPenaltyManager<CFtype>::Print(std::ostream& os) const
{
  os  << "  Min/Max shift: " << min_shift << " / " << max_shift << std::endl;
  os  << "  Current shift: " << shift << std::endl;
}

template <typename CFtype>
void ComplexShiftingPenaltyManager<CFtype>::Print(std::ostream& os) const
{
  os  << "  Max Feasible/Infeasible iterations: " << max_feasible_iter << " / " << max_infeasible_iter << std::endl;
  ShiftingPenaltyManager<CFtype>::Print(os);
}

template <typename CFtype>
bool ComplexShiftingPenaltyManager<CFtype>::Reset()
{
  feasible_iter = 0;
  infeasible_iter = 0;
  if (this->shift != this->start_shift)
    {
      this->shift = this->start_shift;
      return true;
    }
  else
    return false;
}

template <typename CFtype>
bool ComplexShiftingPenaltyManager<CFtype>::Update(CFtype cost)
{
  bool update = false;
  float perturb = Random::Double(this->min_perturb, this->max_perturb);
  if (cost <= this->cost_threshold)
    {
      feasible_iter++;
      infeasible_iter = 0;
      if (feasible_iter == max_feasible_iter)
        {
	  if (this->shift > this->min_shift)
            {
	      this->shift /= perturb;
	      update = true;
	      if (this->shift < this->min_shift)
		{		
		  this->shift = this->min_shift;
		}
            }
	  feasible_iter = 0;
        }
      if (cost < this->cost_threshold)
	{
	  this->cost_threshold = cost;
	}
    }
  else
    {
      infeasible_iter++;
      feasible_iter = 0;
      if (infeasible_iter == max_infeasible_iter)
        {
	  if (this->shift < this->max_shift)
            {
	      this->shift *= perturb;
	      update = true;
	      if (this->shift > this->max_shift)
		{
		  this->shift = this->max_shift;
		}
	    }
	  infeasible_iter = 0;
        }
    }
  return update;
}

template <typename CFtype>
bool SimpleShiftingPenaltyManager<CFtype>::Reset()
{
  bool reset;
  if (this->shift != this->start_shift)
    {
      this->shift = this->start_shift;
      reset = true;
    }
  else
    reset = false;
  if (this->observer != NULL)
    this->observer->NotifyReset(*this);
  
  return reset;
}


template <typename CFtype>
bool SimpleShiftingPenaltyManager<CFtype>::Update(CFtype cost)
{
  bool update = false;
  float perturb = Random::Double(this->min_perturb, this->max_perturb);
  if (cost <= this->cost_threshold)
    {
      if (this->shift > this->min_shift)
	{
	  this->shift /= perturb;
	  update = true;
	  if (this->shift < this->min_shift)
	    this->shift = this->min_shift;
	}
      if (cost < this->cost_threshold)
	{	 
	  this->cost_threshold = cost;
	  if (this->observer != NULL)
	    this->observer->NotifyNewThreshold(*this);
	}
    }
  else
    {
      if (this->shift < this->max_shift)
	{
	  this->shift *= perturb;
	  update = true;
	  if (this->shift > this->max_shift)
	    this->shift = this->max_shift;
        }
    }
  if (this->observer != NULL)
    this->observer->NotifyUpdate(*this, cost);

  return update;
}

template <typename CFtype>
void ComplexShiftingPenaltyManager<CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  double perturb_level;
  os << "  COMPLEX SHIFTING PENALTY PARAMETERS" << std::endl;
  os << "    Perturb level: ";
  is >> perturb_level;
  this->SetPerturbValue(perturb_level);
  os << "    Number of feasible iterations: ";
  is >> max_feasible_iter;
  os << "    Number of infeasible iterations: ";
  is >> max_infeasible_iter;
  os << "    Shift range (min,max): ";
  is >> this->min_shift >> this->max_shift;
  os << "    Start shift: ";
  is >> this->start_shift;
  os << "    Cost threshold: ";
  is >> this->cost_threshold;
  this->shift = this->start_shift;
}

template <typename CFtype>
void SimpleShiftingPenaltyManager<CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  double perturb_level;
  os << "  SIMPLE SHIFTING PENALTY PARAMETERS" << std::endl;
  os << "    Perturb level: ";
  is >> perturb_level;
  this->SetPerturbValue(perturb_level);
  os << "    Shift range (min,max): ";
  is >> this->min_shift >> this->max_shift;
  os << "    Start shift: ";
  is >> this->start_shift;
  os << "    Cost threshold: ";
  is >> this->cost_threshold;
  this->shift = this->start_shift;
}


#endif // define _SHIFTING_PENALTY_MANAGER_HH_
