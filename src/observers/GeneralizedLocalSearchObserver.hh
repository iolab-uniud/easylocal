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

#if !defined(_SOLVER_OBSERVER_HH_)
#define _SOLVER_OBSERVER_HH_

template <class Input, class Output, class State, typename CFtype>
class GeneralizedLocalSearch;

template <class Input, class Output, class State, typename CFtype = int>
class GeneralizedLocalSearchObserver
{
public:
  GeneralizedLocalSearchObserver(unsigned notify_level = 2, std::ostream& r_os = std::cout);
  void NotifyRound(GeneralizedLocalSearch<Input,Output,State,CFtype>& s);
  void NotifyRunnerStart(GeneralizedLocalSearch<Input,Output,State,CFtype>& s);
  void NotifyRunnerStop(GeneralizedLocalSearch<Input,Output,State,CFtype>& s);
  void NotifyKickerStart(GeneralizedLocalSearch<Input,Output,State,CFtype>& s);
  void NotifyKickStep(GeneralizedLocalSearch<Input,Output,State,CFtype>& s, const CFtype& c);
  void NotifyKickerStop(GeneralizedLocalSearch<Input,Output,State,CFtype>& s);
  void SetNotifyRunner() {  notify_runner = true; } 
protected:
  bool notify_round, notify_runner, notify_kicker;
  std::ostream& os;
};

template <class Input, class Output, class State, typename CFtype>
GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::GeneralizedLocalSearchObserver(unsigned notify_level, std::ostream& r_os) 
  : os(r_os)
{
  if (notify_level >= 1)
    notify_round = true;
  else
    notify_round = false;
  if (notify_level == 2)  
    {
      notify_runner = true;
      notify_kicker = true;
    }
  else
    {
      notify_runner = false;
      notify_kicker = false;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyRound(GeneralizedLocalSearch<Input,Output,State,CFtype>& s)
{
  if (notify_round)
    {
      os << "Round " << s.rounds << "/" << s.max_rounds << " finished (idle " << s.idle_rounds << "/" << s.max_idle_rounds << ")" << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyKickerStart(GeneralizedLocalSearch<Input,Output,State,CFtype>& s)
{
  if (notify_kicker)
    {
      os << "Start kicker of solver " << s.name << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyKickStep(GeneralizedLocalSearch<Input,Output,State,CFtype>& s, const CFtype& cost)
{
  if (notify_kicker)
    {
      os << "   Kick move, cost: " <<  cost
	 << ", time " << s.chrono.TotalTime()
	 << ", step " << s.p_kicker->Step() << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyKickerStop(GeneralizedLocalSearch<Input,Output,State,CFtype>& s)
{
  if (notify_kicker)
    {
      os << "Stop kicker. Cost : " << s.best_state_cost << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyRunnerStart(GeneralizedLocalSearch<Input,Output,State,CFtype>& s)
{
  if (notify_runner)
    {
      os << "Starting runner " << s.current_runner << " of solver " << s.name << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyRunnerStop(GeneralizedLocalSearch<Input,Output,State,CFtype>& s)
{
  if (notify_runner)
    {
      os << "Runner: " << s.current_runner << ", cost: " << s.runners[s.current_runner]->GetStateCost() 
	 << ", distance from starting/best states " << s.sm.StateDistance(s.current_state, s.runners[s.current_runner]->GetState())
	 << "/" << s.sm.StateDistance(s.best_state, s.runners[s.current_runner]->GetState())
	 << " (" << s.runners[s.current_runner]->GetIterationsPerformed() << " iterations, time " << s.chrono.TotalTime() 
	 << "), Rounds " << s.rounds << "/" << s.max_rounds << ", Idle rounds " << s.idle_rounds << "/" << s.max_idle_rounds << std::endl;
    }
}


#endif // define _SOLVER_OBSERVER_HH_
