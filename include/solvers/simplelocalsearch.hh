#pragma once

#include <future>

#include "helpers/statemanager.hh"
#include "solvers/abstractlocalsearch.hh"
#include "runners/runner.hh"

namespace EasyLocal
{
  namespace Core
  {
    /** The Simple Local Search solver handles a simple local search algorithm
     encapsulated in a runner.
     @ingroup Solvers
     */
    template <class Input, class State, class CostStructure>
    class SimpleLocalSearch
    : public AbstractLocalSearch<Input, State, CostStructure>
    {
    public:
      using Runner = Runner<Input, State, CostStructure>;
      
      using AbstractLocalSearch<Input, State, CostStructure>::AbstractLocalSearch;
      
      /**
       Sets the runner employed for solving the problem to the one passed as
       parameter.
       
       @param r the new runner to be used
       */
      void SetRunner(Runner &r)
      {
        this->p_runner = &r;
      }
      
      void Print(std::ostream &os = std::cout) const
      {
        os << "Simple Local Search Solver: " << this->name << std::endl;
        if (this->p_runner)
          this->p_runner->Print(os);
        else
          os << "<no runner attached>" << std::endl;
      }
      
      void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout)
      {
        os << "Simple Local Search Solver: " << this->name << " parameters" << std::endl;
        os << "Runner: " << std::endl;
        if (this->p_runner)
          this->p_runner->ReadParameters(is, os);
      }
      
      virtual std::shared_ptr<State> GetCurrentState() const
      {
        return this->p_runner->GetCurrentBestState();
      }
    protected:
      void Go(const Input& in)
      {
        if (!p_runner)
          // FIXME: add a more specific exception behavior
          throw std::logic_error("Runner not set in object " + this->name);
        this->current_state_cost = p_runner->Go(in, *this->p_current_state);
        
        *this->p_best_state = *this->p_current_state;
        this->best_state_cost = this->current_state_cost;
      }
      
      void AtTimeoutExpired()
      {
        p_runner->Interrupt();
      }
      
      void ResetTimeout()
      {
        AbstractLocalSearch<Input, State, CostStructure>::ResetTimeout();
        this->p_runner->ResetTimeout();
      }
      
      Runner *p_runner; /**< to the managed runner. */
    };
  } // namespace Core
} // namespace EasyLocal
