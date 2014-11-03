#if !defined(_HILL_CLIMBING_HH_)
#define _HILL_CLIMBING_HH_

#include <stdexcept>

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/runners/moverunner.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Hill Climbing runner considers random move selection. A move
     is then performed only if it does improve or it leaves unchanged
     the value of the cost function.
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class HillClimbing : public MoveRunner<Input, State, Move, CFtype>
    {
    public:
      HillClimbing(const Input& in, StateManager<Input, State, CFtype>& e_sm,
                   NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne, std::string name);
      
      Parameter<unsigned long int> max_idle_iterations;
    protected:
      void RegisterParameters();
      bool MaxIdleIterationExpired() const;
      bool MaxIterationExpired() const;
      bool StopCriterion();
      void SelectMove();
      // parameters
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a hill climbing runner by linking it to a state manager,
     a neighborhood explorer, and an input object.
     
     @param s a pointer to a compatible state manager
     @param ne a pointer to a compatible neighborhood explorer
     @param in a pointer to an input object
     */
    
    template <class Input, class State, class Move, typename CFtype>
    HillClimbing<Input, State, Move, CFtype>::HillClimbing(const Input& in,
                                                           StateManager<Input, State, CFtype>& e_sm,
                                                           NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                           std::string name)
    : MoveRunner<Input, State, Move, CFtype>(in, e_sm, e_ne, name, "Hill Climbing Runner")
    {}
    
    
    template <class Input, class State, class Move, typename CFtype>
    void HillClimbing<Input, State, Move, CFtype>::RegisterParameters()
    {
      MoveRunner<Input, State, Move, CFtype>::RegisterParameters();
      max_idle_iterations("max_idle_iterations", "Total number of allowed idle iterations", this->parameters);
    }
    
    /**
     The select move strategy for the hill climbing simply looks for a
     random move that improves or leaves the cost unchanged.
     */
    template <class Input, class State, class Move, typename CFtype>
    void HillClimbing<Input, State, Move, CFtype>::SelectMove()
    {
      // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
      const size_t samples = 10;
      size_t sampled;
      EvaluatedMove<Move, CFtype> em = this->ne.RandomFirst(*this->p_current_state, samples, sampled, [](const Move& mv, CostComponents<CFtype> move_cost) {
        if (move_cost.is_weighted)
          return LessThanOrEqualTo(move_cost.weighted, 0.0);
        else
          return LessThanOrEqualTo(move_cost.total, (CFtype)0);
      }, this->weights);
      this->current_move = em;
      this->evaluations += sampled;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    bool HillClimbing<Input, State, Move, CFtype>::MaxIdleIterationExpired() const
    {
      return this->iteration - this->iteration_of_best >= this->max_idle_iterations;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    bool HillClimbing<Input, State, Move, CFtype>::MaxIterationExpired() const
    {
      return this->iteration >= this->max_iterations;
    }
    
    /**
     The stop criterion is based on the number of iterations elapsed from
     the last strict improvement of the best state cost.
     */
    template <class Input, class State, class Move, typename CFtype>
    bool HillClimbing<Input, State, Move, CFtype>::StopCriterion()
    {
      return MaxIdleIterationExpired() || this->MaxIterationExpired();
    }
  }
}

#endif // _HILL_CLIMBING_HH_
