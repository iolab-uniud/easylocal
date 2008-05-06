#ifndef _SOLVER_OBSERVER_HH_
#define _SOLVER_OBSERVER_HH_

template <class Input, class Output, class State, typename CFtype>
class GeneralizedLocalSearchSolver;

template <class Input, class Output, class State, typename CFtype = int>
class GeneralizedLocalSearchObserver
{
public:
  GeneralizedLocalSearchObserver(unsigned notify_level = 2, std::ostream& r_os = std::cerr);
  void NotifyRound(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s);
  void NotifyRunnerStart(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s);
  void NotifyRunnerStop(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s);
  void NotifyKickerStart(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s);
  void NotifyKickStep(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s, const CFtype& c);
  void NotifyKickerStop(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s);
  void SetNotifyRunner() {  notify_runner = true; } 
  //... FIXME
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
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyRound(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s)
{
  if (notify_round)
    {
      
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyKickerStart(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s)
{
  if (notify_kicker)
    {
      os << "Start kicker of solver " << s.name << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyKickStep(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s, const CFtype& cost)
{
  if (notify_kicker)
    {
      os << "   Kick move, cost: " <<  cost
	 << ", time " << s.chrono.TotalTime()
	 << ", step " << s.p_kicker->Step() << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyKickerStop(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s)
{
  if (notify_kicker)
    {
      os << "Stop kicker" << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyRunnerStart(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s)
{
  if (notify_runner)
    {
      os << "Starting runner " << s.current_runner << " of solver " << s.name << std::endl;
    }
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchObserver<Input,Output,State,CFtype>::NotifyRunnerStop(GeneralizedLocalSearchSolver<Input,Output,State,CFtype>& s)
{
  if (notify_runner)
    {
      os << "Runner: " << s.current_runner << ", cost: " << s.runners[s.current_runner]->GetStateCost() 
	 << ", distance from current " << s.sm.StateDistance( s.current_state, s.runners[s.current_runner]->GetState())
	 << ", distance from (previous) best " << s.sm.StateDistance( s.best_state, s.runners[s.current_runner]->GetState())
	 << " (" << s.runners[s.current_runner]->GetIterationsPerformed() << " iterations, time " << s.chrono.TotalTime() 
	 << "), Rounds " << s.rounds << "/" << s.max_rounds << ", Idle rounds " << s.idle_rounds << "/" << s.max_idle_rounds << std::endl;
    }
}


#endif /*OBSERVER_HH_*/
