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

#if !defined(_RUNNER_OBSERVER_HH_)
#define _RUNNER_OBSERVER_HH_

#include <utils/Chronometer.hh>

template <class Input, class State, class Move, typename CFtype>
class MoveRunner;

template <class Input, class State, class Move, typename CFtype = int>
class RunnerObserver
{
public:
  RunnerObserver(unsigned verbosity_level, unsigned plot_level, std::ostream& log_os = std::cout, std::ostream& plot_os = std::cout);
  virtual ~RunnerObserver() {}
  virtual void NotifyStartRunner(MoveRunner<Input,State,Move,CFtype>& r);
  virtual void NotifyNewBest(MoveRunner<Input,State,Move,CFtype>& r);
  virtual void NotifyStoreMove(MoveRunner<Input,State,Move,CFtype>& r);
  virtual void NotifyEndRunner(MoveRunner<Input,State,Move,CFtype>& r);
protected:
  bool notify_new_best, notify_store_move, plot_improving_moves, plot_all_moves;
  std::ostream &log, &plot;
};

template <class Input, class State, class Move, typename CFtype>
RunnerObserver<Input,State,Move,CFtype>::RunnerObserver(unsigned verbosity_level, unsigned plot_level, std::ostream& log_os, std::ostream& plot_os) 
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

template <class Input, class State, class Move, typename CFtype>
void RunnerObserver<Input,State,Move,CFtype>::NotifyStartRunner(MoveRunner<Input,State,Move,CFtype>& r)
{
  if (plot_improving_moves || plot_all_moves)
    plot << r.number_of_iterations << ' ' << r.chrono.TotalTime() << ' ' << r.current_state_cost << std::endl;
}


template <class Input, class State, class Move, typename CFtype>
void RunnerObserver<Input,State,Move,CFtype>::NotifyNewBest(MoveRunner<Input,State,Move,CFtype>& r)
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

template <class Input, class State, class Move, typename CFtype>
void RunnerObserver<Input,State,Move,CFtype>::NotifyStoreMove(MoveRunner<Input,State,Move,CFtype>& r)
{
  if (notify_store_move)
  {
    log << "Move: " << r.current_move << ", Move Cost: " << r.current_move_cost << " (current: "
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
    plot << r.name << ' '<< r.number_of_iterations << ' ' << r.chrono.TotalTime() << ' ' << r.current_state_cost << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
void RunnerObserver<Input,State,Move,CFtype>::NotifyEndRunner(MoveRunner<Input,State,Move,CFtype>& r)
{
  if (plot_improving_moves || plot_all_moves)
    plot << r.number_of_iterations << ' ' << r.chrono.TotalTime() << ' ' << r.current_state_cost << std::endl;
}

#endif /*OBSERVER_HH_*/
