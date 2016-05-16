#pragma once

#include "easylocal/solvers/abstractlocalsearch.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Variable Neighborhood Descent solver handles a VND algorithm
     implemented through a Kicker.
     @ingroup Solvers
     */
    template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
    class VariableNeighborhoodDescent
    : public AbstractLocalSearch<Input, Output, State, CostStructure>
    {
    public:
      VariableNeighborhoodDescent(const Input& in,
                                  StateManager<Input, State, CostStructure>& e_sm,
                                  OutputManager<Input, Output, State, CostStructure>& e_om,
                                  unsigned int max_k,
                                  std::string name);
      
      // void SetKicker(Kicker<Input, State, CostStructure>& k);
      
    protected:
      void Go();
      // Kicker<Input, State, CostStructure>* p_kicker; /**< A pointer to the managed kicker. */
      unsigned int max_k;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a variable neighborhood descent solver by providing it links to
     a state manager, an output manager, a kicker, an input,
     and an output object.
     
     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @param in a pointer to an input object
     @param out a pointer to an output object
     */
    template <class Input, class Output, class State, class CostStructure>
    VariableNeighborhoodDescent<Input, Output, State, CostStructure>::VariableNeighborhoodDescent(const Input& in,
                                                                                           StateManager<Input, State, CostStructure>& e_sm,
                                                                                           OutputManager<Input, Output, State, CostStructure>& e_om,
                                                                                           unsigned int max_k,
                                                                                           std::string name)
    : AbstractLocalSearch<Input, Output, State, CostStructure>(in, e_sm, e_om, name, "Variable Neighborhood Descent Solver")
    {
      p_kicker = nullptr;
      this->max_k = max_k;
    }
    
    /**
     Sets the runner employed for solving the problem to the one passed as
     parameter.
     
     @param r the new runner to be used
     */
    template <class Input, class Output, class State, class CostStructure>
    void VariableNeighborhoodDescent<Input, Output, State, CostStructure>::SetKicker(Kicker<Input, State, CostStructure>& k)
    { this->p_kicker = &k; }
    
    template <class Input, class Output, class State, class CostStructure>
    void VariableNeighborhoodDescent<Input, Output, State, CostStructure>::Go()
    {
      unsigned int k = 1;
      CFtype kick_cost;
      do
      {
        this->p_kicker->SetStep(k);
        std::cerr << "Selected Kick: " << k << " ";
        kick_cost = this->p_kicker->FirstImprovingKick(*this->p_current_state);
        std::cerr << kick_cost << std::endl;
        if (LessThan(kick_cost, 0))
        {
          this->p_kicker->MakeKick(*this->p_current_state);
          this->current_state_cost += kick_cost;
          std::cerr << "Performed Kick: [" << this->current_state_cost << "]: " << k << std::endl;
          this->p_kicker->PrintKick(std::cerr);
          k = 1;
        }
        else
          k++;
      }
      while (k <= this->max_k && !this->sm.LowerBoundReached(this->current_state_cost));
    }
  }
}

