#if !defined(_SIMPLE_KICKER_OBSERVER_HH_)
#define _SIMPLE_KICKER_OBSERVER_HH_

namespace EasyLocal {
    
  namespace Extra {
    template <class Input, class State, class Move, typename CFtype>
    class SimpleKicker;
  }
  
  using namespace Extra;
    
  namespace Debug {

    template <class Input, class State, class Move, typename CFtype = int>
    class SimpleKickerObserver
    {
    public:
      SimpleKickerObserver(unsigned int verbosity_level, std::ostream& log_os = std::cout);
      void NotifyStartKicking(SimpleKicker<Input,State,Move,CFtype>& k);
      void NotifyNewKick(SimpleKicker<Input,State,Move,CFtype>& k);
      void NotifyBestKick(SimpleKicker<Input,State,Move,CFtype>& k);
      void NotifyStopKicking(SimpleKicker<Input,State,Move,CFtype>& k);
    protected:
      bool notify_new_best, notify_new_kick;
      int total_kicks, improving_kicks, sideways_kicks;
      std::ostream &log;
    };

    template <class Input, class State, class Move, typename CFtype>
    SimpleKickerObserver<Input,State,Move,CFtype>::SimpleKickerObserver(unsigned int verbosity_level, std::ostream& log_os) 
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

    template <class Input, class State, class Move, typename CFtype>
    void SimpleKickerObserver<Input,State,Move,CFtype>::NotifyStartKicking(SimpleKicker<Input,State,Move,CFtype>& k)
    {
      log << "Start kicking: (step " << k.Step() << ")" << std::endl;
      total_kicks = 0;
      improving_kicks = 0;
      sideways_kicks = 0;
    }


    template <class Input, class State, class Move, typename CFtype>
    void SimpleKickerObserver<Input,State,Move,CFtype>::NotifyBestKick(SimpleKicker<Input,State,Move,CFtype>& k)
    {
      if (notify_new_best)
      {
        log << "--New best kick: " << k.best_kick_cost << " (";
        for (unsigned int i = 0; i < k.step; i++)
        {
          log << k.internal_best_moves[i];
          if (i < k.step - 1) 
            log << ", ";
        }
        log << ')' << std::endl;
      }
    }

    template <class Input, class State, class Move, typename CFtype>
    void SimpleKickerObserver<Input,State,Move,CFtype>::NotifyNewKick(SimpleKicker<Input,State,Move,CFtype>& k)
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
          log << k.current_moves[i];
          if (i < k.step - 1) 
            log << ',';
        }
        log << ')' << std::endl;
      }
    }

    template <class Input, class State, class Move, typename CFtype>
    void SimpleKickerObserver<Input,State,Move,CFtype>::NotifyStopKicking(SimpleKicker<Input,State,Move,CFtype>& k)
    {
      log << "Total number of kicks : " << total_kicks << std::endl
        << "Improving kicks : " << improving_kicks << " (" << improving_kicks*100.0/total_kicks << "%)" << std::endl
          << "Sideways kicks : " << sideways_kicks << " (" << sideways_kicks*100.0/total_kicks << "%)" << std::endl
            << "End kicking" << std::endl;
      total_kicks = 0;
      improving_kicks = 0;
      sideways_kicks = 0;
    }        
  }
}

#endif // _SIMPLE_KICKER_OBSERVER_HH_
