// $Id: GRASP.hh 252 2008-12-01 14:35:18Z digasper $
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

#if !defined(_GRASP_SOLVER_HH_)
#define _GRASP_SOLVER_HH_

#include <solvers/AbstractLocalSearch.hh>
#include <kickers/Kicker.hh>
#include <vector>


/** An Iterated Local Search solver handles both a runner encapsulating a local
    search algorithm and a kicker used for perturbing current solution.
    @ingroup Solvers */
template <class Input, class Output, class State, typename CFtype = int>
class GRASP
  : public AbstractLocalSearch<Input,Output,State,CFtype>
{
public:
  GRASP(const Input& i,
	StateManager<Input,State,CFtype>& sm,
	OutputManager<Input,Output,State,CFtype>& om,
	std::string name);
  void Print(std::ostream& os = std::cout) const;
  void SetRunner(Runner<Input,State,CFtype>& r);
  unsigned int GetRestarts() const { return restarts; }
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);

  void Solve(double alpha, unsigned int k, unsigned int restarts);
protected:
  unsigned int restarts;

  Runner<Input,State,CFtype>* runner; /**< The linked runner. */
  Chronometer chrono;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs an iterated local search solver by providing it links to
   a state manager, an output manager, a runner, a kicker, an input,
   and an output object.
 
   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param r a pointer to a compatible runner
   @param k a pointer to a compatible kicker
   @param in a pointer to an input object
   @param out a pointer to an output object
*/
template <class Input, class Output, class State, typename CFtype>
GRASP<Input,Output,State,CFtype>::GRASP(const Input& i,
					StateManager<Input,State,CFtype>& sm,
					OutputManager<Input,Output,State,CFtype>& om,
					std::string name)
  : AbstractLocalSearch<Input,Output,State,CFtype>(i, sm, om, name)
{
  runner = NULL;
}

/**
   Set the runner.
 
   @param r a pointer to a compatible runner to add
*/
template <class Input, class Output, class State, typename CFtype>
void GRASP<Input,Output,State,CFtype>::SetRunner(Runner<Input,State,CFtype>& r)
{
  runner = &r;
}

// template <class Input, class Output, class State, typename CFtype>
// void GRASP<Input,Output,State,CFtype>::Print(std::ostream& os) const
// {
//   os  << "Generalized Local Search Solver: " << this->name << std::endl;
	
//   if (this->runners.size() > 0)
//     for (unsigned int i = 0; i < this->runners.size(); i++)
//       {
// 	os  << "Runner[" << i << "]" << std::endl;
// 	this->runners[i]->Print(os);
//       }
//   else
//     os  << "<no runner attached>" << std::endl;
//   if (p_kicker)
//     p_kicker->Print(os);
//   else
//     os  << "<no kicker attached>" << std::endl;
//   os << "Max idle rounds: " << max_idle_rounds << std::endl;
//   os << "Timeout " << this->timeout << std::endl;
// }

/**
   Solves using a single runner
*/
template <class Input, class Output, class State, typename CFtype>
void GRASP<Input,Output,State,CFtype>::Solve(double alpha, unsigned int k, unsigned int trials)
{
  bool timeout_expired = false;
  unsigned t;
  if (runner == NULL)
    throw std::logic_error("No runner set for solver " + this->name);
  chrono.Reset();
  chrono.Start();
  for (t = 0; t < trials; t++)
    {
      this->sm.GreedyState(this->current_state, alpha, k);
      runner->SetState(this->current_state);
      timeout_expired = LetGo(*runner);

      this->current_state = runner->GetState();
      this->current_state_cost = runner->GetStateCost();

      if (t == 0 || LessThan(this->current_state_cost, this->best_state_cost))
	{
	  this->best_state = this->current_state;
	  this->best_state_cost = this->current_state_cost;
	  if (this->sm.LowerBoundReached(this->best_state_cost))
	    break;
	}
      if (timeout_expired)
	break;
    }
  chrono.Stop();
}

template <class Input, class Output, class State, typename CFtype>
void GRASP<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "GRASP Solver: " << this->name << " parameters" << std::endl;
  os << "Runner: " << std::endl;
	
  this->runner->ReadParameters(is, os);

#if defined(HAVE_PTHREAD)
  double timeout;
  os << "Timeout: ";
  is >> timeout;
  this->SetTimeout(timeout);
#endif 
}


#endif // _GRASP_SOLVER_HH_
