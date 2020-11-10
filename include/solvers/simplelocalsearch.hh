#pragma once

#include <future>

#include "helpers/solutionmanager.hh"
#include "solvers/localsearch.hh"
#include "runners/runner.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The Simple Local Search solver handles a simple local search algorithm
     encapsulated in a runner.
     @ingroup Solvers
     */
    template <class Input, class Solution, class CostStructure = DefaultCostStructure<int>>
    class SimpleLocalSearch
    : public LocalSearch<Input, Solution, CostStructure>
    {
    public:
      typedef Runner<Input, Solution, CostStructure> RunnerType;
      
      using LocalSearch<Input, Solution, CostStructure>::LocalSearch;
      
      void SetRunner(Runner<Input, Solution, CostStructure> &r);
      Runner<Input, Solution, CostStructure>* GetRunner() const { return p_runner; }
      void Print(std::ostream &os = std::cout) const;
      void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout);
      virtual std::shared_ptr<Solution> GetCurrentState() const;
      
    protected:
      void Go();
      void AtTimeoutExpired();
      void ResetTimeout();
      
      RunnerType *p_runner; /**< to the managed runner. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Solution, class CostStructure>
    void SimpleLocalSearch<Input, Solution, CostStructure>::ReadParameters(std::istream &is, std::ostream &os)
    {
      os << "Simple Local Search Solver: " << this->name << " parameters" << std::endl;
      os << "Runner: " << std::endl;
      if (this->p_runner)
        this->p_runner->ReadParameters(is, os);
    }
    
    template <class Input, class Solution, class CostStructure>
    void SimpleLocalSearch<Input, Solution, CostStructure>::Print(std::ostream &os) const
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
    template <class Input, class Solution, class CostStructure>
    void SimpleLocalSearch<Input, Solution, CostStructure>::SetRunner(Runner<Input, Solution, CostStructure> &r)
    {
      this->p_runner = &r;
    }
    
    template <class Input, class Solution, class CostStructure>
    void SimpleLocalSearch<Input, Solution, CostStructure>::Go()
    {
      if (!p_runner)
        // FIXME: add a more specific exception behavior
        throw std::logic_error("Runner not set in object " + this->name);
      this->current_state_cost = p_runner->Go(*this->p_current_state);
      
      *this->p_best_state = *this->p_current_state;
      this->best_state_cost = this->current_state_cost;
    }
    
    template <class Input, class Solution, class CostStructure>
    void SimpleLocalSearch<Input, Solution, CostStructure>::AtTimeoutExpired()
    {
      p_runner->Interrupt();
    }
    
    template <class Input, class Solution, class CostStructure>
    void SimpleLocalSearch<Input, Solution, CostStructure>::ResetTimeout()
    {
      Interruptible<int>::ResetTimeout();
      this->p_runner->ResetTimeout();
    }
    
    template <class Input, class Solution, class CostStructure>
    std::shared_ptr<Solution> SimpleLocalSearch<Input, Solution, CostStructure>::GetCurrentState() const
    {
      return this->p_runner->GetCurrentBestState();
    }
  } // namespace Core
} // namespace EasyLocal
