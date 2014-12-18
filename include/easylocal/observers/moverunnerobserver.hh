#if !defined(_MOVE_RUNNER_OBSERVER_HH_)
#define _MOVE_RUNNER_OBSERVER_HH_

#include "easylocal/runners/moverunner.hh"
#include <chrono>
#include <iostream>

namespace EasyLocal {
  namespace Debug {
    
    template <class Input, class State, class Move, typename CFtype = int>
    class MoveRunnerObserver
    {
    protected:
      typedef typename EasyLocal::Core::MoveRunner<Input, State, Move, CFtype>::Event Event;
    public:
      MoveRunnerObserver(std::ostream& os = std::cout);
      void operator()(Event event, CostStructure<CFtype> current_state_cost, const EvaluatedMove<Move, CFtype>& em, const std::string& status_string) const;
      unsigned int events() const
      {
        return Event::START | Event::NEW_BEST;
      }
      mutable std::chrono::high_resolution_clock::time_point start;
      std::ostream& os;
    };
    
    template <class Input, class State, class Move, typename CFtype>
    MoveRunnerObserver<Input, State, Move, CFtype>::MoveRunnerObserver(std::ostream& os) : os(os)
    {}
    
    template <class Input, class State, class Move, typename CFtype>
    void MoveRunnerObserver<Input, State, Move, CFtype>::operator()(Event event, CostStructure<CFtype> current_state_cost, const EvaluatedMove<Move, CFtype>& em, const std::string& status_string) const
    {
      switch (event)
      {
        case Event::START:
          start = std::chrono::high_resolution_clock::now();
          break;
        case Event::NEW_BEST:
          os << "--New Best " << current_state_cost << " [" << em.cost.total << "] (" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count() / 1000.0 << "s)" << " " << status_string << std::endl;
          start = std::chrono::high_resolution_clock::now();
          break;
        default:
          break;
      }
    }
  }
}


#endif