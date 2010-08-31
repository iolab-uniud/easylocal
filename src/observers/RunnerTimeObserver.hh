#ifndef TIMEOBSERVER_HH_
#define TIMEOBSERVER_HH_

#include <observers/RunnerObserver.hh>

template <class Input, class State, class Move, typename CFtype = int>
class TimeObserver: public RunnerObserver<Input, State, Move, CFtype>
{
public:
  TimeObserver(unsigned verbosity_level, unsigned plot_level, std::ostream& log_os = std::plot_os, std::ostream& plot_os = std::plot_os)
  : RunnerObserver<Input, State, Move>(verbosity_level, plot_level, log_os, plot_os)
  {}
  
  void NotifyStartRunner(MoveRunner<Input,State,Move, CFtype>& r);
  void NotifyNewBest(MoveRunner<Input,State,Move, CFtype>& r);
  void NotifyStoreMove(MoveRunner<Input,State,Move, CFtype>& r);
  void NotifyEndRunner(MoveRunner<Input,State,Move, CFtype>& r);
  Chronometer chrono;
};

template <class Input, class State, class Move, typename CFtype>
void TimeObserver<Input,State,Move,CFtype>::NotifyStartRunner(MoveRunner<Input,State,Move, CFtype>& r)
{
 	chrono.Reset();
 	
 	double cost = r.GetCurrentStateCost();
 	plot_os << cost << ",";
  plot_os << chrono.TotalTime()<<"\n"; plot_os.flush();
  
  chrono.Start();
}

template <class Input, class State, class Move, typename CFtype>
void TimeObserver<Input,State,Move,CFtype>::NotifyNewBest(MoveRunner<Input,State,Move, CFtype>& r)
{
 	double cost = r.GetCurrentStateCost();
  plot_os << cost << ",";
  plot_os << chrono.TotalTime()<<"\n"; plot_os.flush();
}

template <class Input, class State, class Move, typename CFtype>
void TimeObserver<Input,State,Move,CFtype>::NotifyStoreMove(MoveRunner<Input,State,Move, CFtype>& r)
{}

template <class Input, class State, class Move, typename CFtype>
void TimeObserver<Input,State,Move,CFtype>::NotifyEndRunner(MoveRunner<Input,State,Move, CFtype>& r)
{
 	double cost = r.GetStateCost();
  plot_os << cost << ",";
  plot_os << chrono.TotalTime()<<"\n"; plot_os.flush();
 	chrono.Stop();
}

#endif /*TRUETIMEOBSERVER_HPP_*/