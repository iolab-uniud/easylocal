#if !defined(_SIMULATED_ANNEALING_WITH_SHIFTING_PENALTY_HH_)
#define _SIMULATED_ANNEALING_WITH_SHIFTING_PENALTY_HH_

#include "easylocal/runners/simulatedannealingiterationbased.hh"

namespace EasyLocal {

  namespace Core {

    /** Implements the Simulated annealing runner with a stop condition
    based on the number of iterations and the shifting penalty mechanism.
 
    @ingroup Runners
    */

    template <class Input, class State, class Move, typename CFtype>
    class SimulatedAnnealingWithShiftingPenalty : public SimulatedAnnealingIterationBased<Input, State, Move, CFtype>
    {
    public:
  
      SimulatedAnnealingWithShiftingPenalty(const Input& in,
      StateManager<Input, State, CFtype>& e_sm,
      NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
      std::string name);
      std::string StatusString();

    protected:
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      void CompleteMove();
      void SelectMove();
      bool AcceptableMove();

      // additional parameters
      double shift, min_shift, shifted_delta_hard_cost, delta_soft_cost;
      Parameter<double> alpha;
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
    SimulatedAnnealingWithShiftingPenalty<Input, State, Move, CFtype>::SimulatedAnnealingWithShiftingPenalty(const Input& in,
    StateManager<Input, State, CFtype>& e_sm,
    NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
    std::string name)
      : SimulatedAnnealingIterationBased<Input, State, Move, CFtype>(in, e_sm, e_ne, name),
    alpha("shifting_penalty_multiplier", "Multiplier for the shifting penalty", this->parameters)
      { }



    /**
    Initializes the run by invoking the companion superclass method, and
    setting the temperature to the start value.
    */
    template <class Input, class State, class Move, typename CFtype>
    void SimulatedAnnealingWithShiftingPenalty<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {

      SimulatedAnnealingIterationBased<Input, State, Move, CFtype>::InitializeRun(); 
      min_shift = 0.01; // 1.0/HARD_WEIGHT;
      shift = 1.0;  
    }

    /**
    Create a string containing the status of the runner
    */
    template <class Input, class State, class Move, typename CFtype>
    std::string SimulatedAnnealingWithShiftingPenalty<Input, State, Move, CFtype>::StatusString()
    {
      std::stringstream status;
      status << "["
        << "Temp = " << this->temperature << " (" << this->start_temperature << "), "
          << "NS = " << this->neighbors_sampled << " (" << this->max_neighbors_sampled << "), "
            << "NA = " << this->neighbors_accepted  << " (" << this->max_neighbors_accepted << "), "
              << "Shift = " << this->shift << " (" << this->min_shift << "), "
                << "Shifted hard cost = " << this->shifted_delta_hard_cost << " (" << this->current_state_violations << ")"
                  << "]";
      return status.str();
    }

    template <class Input, class State, class Move, typename CFtype>
    void SimulatedAnnealingWithShiftingPenalty<Input, State, Move, CFtype>::CompleteMove()
    {
      SimulatedAnnealingIterationBased<Input, State, Move, CFtype>::CompleteMove();
      if (this->current_state_violations > 0)
        shift = std::min(1.0, shift*alpha);
      else
        shift = std::max(min_shift, shift/alpha);
      //  std::cerr << StatusString() << std::endl;
    }

    template <class Input, class State, class Move, typename CFtype>
    void SimulatedAnnealingWithShiftingPenalty<Input, State, Move, CFtype>::SelectMove()
    {
      this->ne.RandomMove(*this->p_current_state, this->current_move);
      delta_soft_cost = this->ne.DeltaObjective(*this->p_current_state, this->current_move);
      this->current_move_violations = this->ne.DeltaViolations(*this->p_current_state, this->current_move);
      shifted_delta_hard_cost =  this->current_move_violations * shift;
      this->current_move_cost = delta_soft_cost + this->current_move_violations;
      
      this->neighbors_sampled++;
    }

    template <class Input, class State, class Move, typename CFtype>
    bool SimulatedAnnealingWithShiftingPenalty<Input, State, Move, CFtype>::AcceptableMove()
    {
      return LessOrEqualThan(shifted_delta_hard_cost + delta_soft_cost, 0.0)
        || (Random::Double() < exp(-(shifted_delta_hard_cost + delta_soft_cost)/this->temperature)); 
    }
  }
}

#endif // _SIMULATED_ANNEALING_HH_
