#if !defined(_BIMODAL_KICKER_OBSERVER_HH_)
#define _BIMODAL_KICKER_OBSERVER_HH_

template <class Input, class State, class Move1, class Move2, typename CFtype>
class BimodalKicker;

template <class Input, class State, class Move1, class Move2, typename CFtype = int>
class BimodalKickerObserver
{
public:
  BimodalKickerObserver(unsigned verbosity_level, std::ostream& log_os = std::cout);
  void NotifyStartKicking(BimodalKicker<Input,State,Move1,Move2,CFtype>& k);
  void NotifyNewKick(BimodalKicker<Input,State,Move1,Move2,CFtype>& k);
  void NotifyBestKick(BimodalKicker<Input,State,Move1,Move2,CFtype>& k);
  void NotifyStopKicking(BimodalKicker<Input,State,Move1,Move2,CFtype>& k);
protected:
  bool notify_new_best, notify_new_kick;
  int total_kicks, improving_kicks, sideways_kicks;
  std::ostream &log;
};

template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalKickerObserver<Input,State,Move1,Move2,CFtype>::BimodalKickerObserver(unsigned verbosity_level, std::ostream& log_os) 
  : log(log_os)
{
  if (verbosity_level >= 1)
    notify_new_best = true;
  else
    notify_new_best = false;
  if (verbosity_level >= 2)
    notify_new_kick = true;
  else
    notify_new_kick = false;
  total_kicks = 0;
  improving_kicks = 0;
  sideways_kicks = 0;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKickerObserver<Input,State,Move1,Move2,CFtype>::NotifyStartKicking(BimodalKicker<Input,State,Move1,Move2,CFtype>& k)
{
  log << "Start kicking: (step " << k.Step() << ")" << std::endl;
  total_kicks = 0;
  improving_kicks = 0;
  sideways_kicks = 0;
}


template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKickerObserver<Input,State,Move1,Move2,CFtype>::NotifyBestKick(BimodalKicker<Input,State,Move1,Move2,CFtype>& k)
{
  if (notify_new_best)
    {
      log << "--New best kick: " << k.best_kick_cost << " (";
      for (unsigned int i = 0; i < k.step; i++)
	{
	  if (k.pattern[i] == MOVE_1)
	    log << k.internal_best_moves1[i];
	  else 
	    log << k.internal_best_moves2[i];
	  if (i < k.step - 1) 
	    log << ", ";
	}
      log << ')' << std::endl;
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKickerObserver<Input,State,Move1,Move2,CFtype>::NotifyNewKick(BimodalKicker<Input,State,Move1,Move2,CFtype>& k)
{
  total_kicks++;
  if (LessThan(k.current_kick_cost,(CFtype)0))
    improving_kicks++;
  else if (EqualTo(k.current_kick_cost,(CFtype)0))
    sideways_kicks++;

  if (notify_new_kick)
    {
      log << "--New kick: (" << k.current_kick_cost;
      for (unsigned int i = 0; i < k.step; i++)
	{
	  if (k.pattern[i] == MOVE_1)
	    log << k.current_moves1[i];
	  else
	    log << k.current_moves2[i];
	  if (i < k.step - 1) 
	    log << ',';
	}
      log << ')' << std::endl;
    }
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalKickerObserver<Input,State,Move1,Move2,CFtype>::NotifyStopKicking(BimodalKicker<Input,State,Move1,Move2,CFtype>& k)
{
  log << "Total number of kicks : " << total_kicks << std::endl
      << "Improving kicks : " << improving_kicks << " (" << improving_kicks*100.0/total_kicks << "%)" << std::endl
      << "Sideways kicks : " << sideways_kicks << " (" << sideways_kicks*100.0/total_kicks << "%)" << std::endl
      << "End kicking" << std::endl;
  total_kicks = 0;
  improving_kicks = 0;
  sideways_kicks = 0;
}


#endif // _BIMODAL_KICKER_OBSERVER_HH_
