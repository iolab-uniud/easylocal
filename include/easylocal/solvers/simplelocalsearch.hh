#pragma once

#include <future>

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/outputmanager.hh"
#include "easylocal/solvers/abstractlocalsearch.hh"
#include "easylocal/runners/runner.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The Simple Local Search solver handles a simple local search algorithm
     encapsulated in a runner.
     @ingroup Solvers
     */
    template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
    class SimpleLocalSearch
    : public AbstractLocalSearch<Input, Output, State, CostStructure>
    {
    public:
      typedef Runner<Input, State, CostStructure> RunnerType;
      
      using AbstractLocalSearch<Input, Output, State, CostStructure>::AbstractLocalSearch;
      
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      SimpleLocalSearch(const Input &in,
                        StateManager<Input, State, CostStructure> &sm,
                        OutputManager<Input, Output, State> &om,
                        std::string name) : AbstractLocalSearch<Input, Output, State, CostStructure>(in, sm, om, name) {}
      
      void SetRunner(Runner<Input, State, CostStructure> &r);
      void Print(std::ostream &os = std::cout) const;
      void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout);
      virtual std::shared_ptr<State> GetCurrentState() const;
      
    protected:
      void Go(const Input& in);
      void AtTimeoutExpired();
      void ResetTimeout();
      
      RunnerType *p_runner; /**< to the managed runner. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Output, class State, class CostStructure>
    void SimpleLocalSearch<Input, Output, State, CostStructure>::ReadParameters(std::istream &is, std::ostream &os)
    {
      os << "Simple Local Search Solver: " << this->name << " parameters" << std::endl;
      os << "Runner: " << std::endl;
      if (this->p_runner)
        this->p_runner->ReadParameters(is, os);
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void SimpleLocalSearch<Input, Output, State, CostStructure>::Print(std::ostream &os) const
    {
      os << "Simple Local Search Solver: " << this->name << std::endl;
      if (this->p_runner)
        this->p_runner->Print(os);
      else
        os << "<no runner attached>" << std::endl;
    }
    
    /**
     Sets the runner employed for solving the problem to the one passed as
     parameter.
     
     @param r the new runner to be used
     */
    template <class Input, class Output, class State, class CostStructure>
    void SimpleLocalSearch<Input, Output, State, CostStructure>::SetRunner(Runner<Input, State, CostStructure> &r)
    {
      this->p_runner = &r;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void SimpleLocalSearch<Input, Output, State, CostStructure>::Go(const Input& in)
    {
      if (!p_runner)
        // FIXME: add a more specific exception behavior
        throw std::logic_error("Runner not set in object " + this->name);
      this->current_state_cost = p_runner->Go(in, *this->p_current_state);
      
      *this->p_best_state = *this->p_current_state;
      this->best_state_cost = this->current_state_cost;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void SimpleLocalSearch<Input, Output, State, CostStructure>::AtTimeoutExpired()
    {
      p_runner->Interrupt();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void SimpleLocalSearch<Input, Output, State, CostStructure>::ResetTimeout()
    {
      AbstractLocalSearch<Input, Output, State, CostStructure>::ResetTimeout();
      this->p_runner->ResetTimeout();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    std::shared_ptr<State> SimpleLocalSearch<Input, Output, State, CostStructure>::GetCurrentState() const
    {
      return this->p_runner->GetCurrentBestState();
    }
  } // namespace Core
} // namespace EasyLocal
