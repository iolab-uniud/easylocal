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

#if !defined(_BIMODAL_RUNNER_OBSERVER_HH_)
#define _BIMODAL_RUNNER_OBSERVER_HH_

#if !defined(MOVE_ENUM)
typedef enum {
  MOVE_1 = 1,
  MOVE_2
} PatternMove;
#define MOVE_ENUM
#endif

template <class Input, class State, class Move1, class Move2,typename CFtype>
class BimodalMoveRunner;

template <class Input, class State, class Move1, class Move2,typename CFtype = int>
class BimodalRunnerObserver
{
public:
  BimodalRunnerObserver(unsigned verbosity_level, unsigned plot_level, std::ostream& log_os = std::cout, std::ostream& plot_os = std::cout);
  void NotifyStartRunner(BimodalMoveRunner<Input,State,Move1,Move2,CFtype>& r);
  void NotifyNewBest(BimodalMoveRunner<Input,State,Move1,Move2,CFtype>& r);
  void NotifyStoreMove(BimodalMoveRunner<Input,State,Move1,Move2,CFtype>& r);
protected:
  bool notify_new_best, notify_store_move, plot_improving_moves, plot_all_moves;
  std::ostream &log, &plot;
};

template <class Input, class State, class Move1, class Move2,typename CFtype>
BimodalRunnerObserver<Input,State,Move1,Move2,CFtype>::BimodalRunnerObserver(unsigned verbosity_level, unsigned plot_level, 
									     std::ostream& log_os, std::ostream& plot_os) 
  : log(log_os), plot(plot_os)
{
  if (verbosity_level >= 1)
    notify_new_best = true;
  else
    notify_new_best = false;
  if (verbosity_level >= 2)
    notify_store_move = true;
  else
    notify_store_move = false;
  if (plot_level >= 1)
    plot_improving_moves = true;
  else
    plot_improving_moves = false;
  if (plot_level >= 2)
    plot_all_moves = true;
  else
    plot_all_moves = false;
}

template <class Input, class State, class Move1, class Move2,typename CFtype>
void BimodalRunnerObserver<Input,State,Move1,Move2,CFtype>::NotifyStartRunner(BimodalMoveRunner<Input,State,Move1,Move2,CFtype>& r)
{
  if (plot_improving_moves || plot_all_moves)
    plot << r.name << ' ' << r.number_of_iterations << ' ' << r.chrono.TotalTime() << ' ' << r.current_state_cost << std::endl;
}


template <class Input, class State, class Move1, class Move2,typename CFtype>
void BimodalRunnerObserver<Input,State,Move1,Move2,CFtype>::NotifyNewBest(BimodalMoveRunner<Input,State,Move1,Move2,CFtype>& r)
{
  if (notify_new_best)
    {
      log << "--New best: " << r.current_state_cost 
	  << " (it: " << r.number_of_iterations << ", idle: " << r.number_of_iterations - r.iteration_of_best 
	  << "), Costs: (";
      for (unsigned int i = 0; i < r.sm.CostComponents(); i++)
	{
	  log << r.sm.Cost(r.current_state,i);
	  if (i < r.sm.CostComponents() - 1) 
	    log << ',';
	}
      log << ')' << std::endl;
    }
  if (plot_improving_moves && !plot_all_moves)
    plot << r.name << ' ' << r.number_of_iterations << ' ' << r.chrono.TotalTime() << ' ' << r.current_state_cost << std::endl;
}

template <class Input, class State, class Move1, class Move2,typename CFtype>
void BimodalRunnerObserver<Input,State,Move1,Move2,CFtype>::NotifyStoreMove(BimodalMoveRunner<Input,State,Move1,Move2,CFtype>& r)
{
  if (notify_store_move)
    {
      if (r.current_move_type == MOVE_1)
        log << "Move (1): " << r.current_move1 << ", Move Cost: " << r.current_move_cost1;
      else
        log << "Move (2): " << r.current_move2 << ", Move Cost: " << r.current_move_cost2;	
      log << " (current: "
      << r.current_state_cost << ", best: " 
      << r.best_state_cost <<  ") it: " << r.number_of_iterations
      << " (idle: " << r.number_of_iterations - r.iteration_of_best << ")" 
      << "), Costs: (";
      for (unsigned int i = 0; i < r.sm.CostComponents(); i++)
	{
	  log << r.sm.Cost(r.current_state,i);
	  if (i < r.sm.CostComponents() - 1) 
	    log << ',';
	}
      log << ')' << std::endl;
    }
  if (plot_all_moves)
    plot << r.name << ' ' << r.number_of_iterations << ' ' << r.chrono.TotalTime() << ' ' << r.current_state_cost << std::endl;
}


#endif /*OBSERVER_HH_*/
