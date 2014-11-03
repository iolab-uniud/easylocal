#if !defined(_GREAT_DELUGE_HH_)
#define _GREAT_DELUGE_HH_

#include <stdexcept>

#include "easylocal/runners/moverunner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Great Deluge runner relies on a probabilistic local
     search technique whose name comes from ... the Bible?
     
     The solver is initialized with a minimum water level, at each
     step a candidate move is generated at random, the move is
     accepted if its quality is greater than the water level. After
     the number of neighbors have been sampled at a certain water
     level, the water level is updated.
     The algorithm stops if we have reached the maximum water level
     or if we have done a certain number of non-improving solutions.
     
     In the implementation, the concept of water levels is reversed.
     
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class GreatDeluge : public MoveRunner<Input, State, Move, CFtype>
    {
    public:
      
      GreatDeluge(const Input& in,
                  StateManager<Input, State, CFtype>& e_sm,
                  NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                  std::string name);
      
    protected:
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      bool StopCriterion();
      void UpdateIterationCounter();
      void SelectMove();
      
      // parameters
      Parameter<double> initial_level; /**< The initial level. */
      Parameter<double> min_level; /**< The minimum level. */
      Parameter<double> level_rate; /**< The level decreasing rate. */
      Parameter<unsigned int> neighbors_sampled; /**< The number of neighbors sampled. */
      // state
      double level; /**< The current level. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a simulated annealing runner by linking it to a state manager,
     a neighborhood explorer, and an input object.
     
     @param s a pointer to a compatible state manager
     @param ne a pointer to a compatible neighborhood explorer
     @param in a poiter to an input object
     */
    
    template <class Input, class State, class Move, typename CFtype>
    GreatDeluge<Input, State, Move, CFtype>::GreatDeluge(const Input& in,
                                                         StateManager<Input, State, CFtype>& e_sm,
                                                         NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                         std::string name)
    : MoveRunner<Input, State, Move, CFtype>(in, e_sm, e_ne, name, "Great Deluge"),
    // parameters
    initial_level("initial_level", "Initial water level", this->parameters),
    min_level("min_level", "Minimum water level", this->parameters),
    level_rate("level_rate", "Water decrease factor", this->parameters),
    neighbors_sampled("neighbors_sampled", "Number of neighbors sampled at each water level", this->parameters)
    {
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting current level to the initial one.
     */
    template <class Input, class State, class Move, typename CFtype>
    void GreatDeluge<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      level = initial_level * this->current_state_cost.total;
    }
    
    /**
     A move is randomly picked and its cost is stored.
     */
    template <class Input, class State, class Move, typename CFtype>
    void GreatDeluge<Input, State, Move, CFtype>::SelectMove()
    {
      // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
      const size_t samples = 10;
      size_t sampled;
      CFtype cur_cost = this->current_state_cost.total;
      double l = level;
      EvaluatedMove<Move, CFtype> em = this->ne.RandomFirst(*this->p_current_state, samples, sampled, [cur_cost, l](const Move& mv, CostComponents<CFtype> move_cost) {
        return LessThanOrEqualTo(move_cost.total, (CFtype)0) || LessThanOrEqualTo((double)(move_cost.total + cur_cost), l);
      });
      this->current_move = em;
    }
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class State, class Move, typename CFtype>
    bool GreatDeluge<Input, State, Move, CFtype>::StopCriterion()
    {
      return level < min_level * this->best_state_cost.total;
    }
    
    /**
     At regular steps, the temperature is decreased
     multiplying it by a cooling rate.
     */
    template <class Input, class State, class Move, typename CFtype>
    void GreatDeluge<Input, State, Move, CFtype>::UpdateIterationCounter()
    {
      MoveRunner<Input, State, Move, CFtype>::UpdateIterationCounter();
      if (this->number_of_iterations % neighbors_sampled == 0)
        level *= level_rate;
    }
  }
}

#endif // _GREAT_DELUGE_HH_
