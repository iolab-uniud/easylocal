#prgama once

#include <vector>

#include "solvers/abstractlocalsearch.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** An Iterated Local Search solver handles both a runner encapsulating a local
     search algorithm and a kicker used for perturbing current solution.
     @ingroup Solvers */
    template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
    class GRASP
    : public AbstractLocalSearch<Input, Output, State, CostStructure>
    {
    public:
      using AbstractLocalSearch<Input, Output, State, CostStructure>::AbstractLocalSearch;
      
      void Print(std::ostream &os = std::cout) const;
      void SetRunner(Runner<Input, State, CostStructure> &r);
      unsigned int GetRestarts() const { return restarts; }
      void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout);
      
      void Go(const Input& in);
      
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      virtual std::shared_ptr<State> GetCurrentState() const;
      
    protected:
      double alpha;
      unsigned int k;
      unsigned int restarts;
      
      Runner<Input, State, CostStructure> *runner; /**< The linked runner. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a GRASP solver by providing it links to
     a state manager, an output manager, a runner, a kicker, an input,
     and an output object.
     
     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @deprecated
     */
    template <class Input, class Output, class State, class CostStructure>
    GRASP<Input, Output, State, CostStructure>::GRASP(const Input &in,
                                                      StateManager<Input, State, CostStructure> &sm,
                                                      OutputManager<Input, Output, State> &om,
                                                      double alpha,
                                                      unsigned int k,
                                                      unsigned int restarts,
                                                      std::string name)
    : AbstractLocalSearch<Input, Output, State, CostStructure>(in, sm, om, name), alpha(alpha), k(k), restarts(restarts)
    {
      runner = nullptr;
    }
    
    /**
     Set the runner.
     
     @param r a pointer to a compatible runner to add
     */
    template <class Input, class Output, class State, class CostStructure>
    void GRASP<Input, Output, State, CostStructure>::SetRunner(Runner<Input, State, CostStructure> &r)
    {
      runner = &r;
    }
    
    // template <class Input, class Output, class State, class CostStructure>
    // void GRASP<Input, Output, State, CostStructure>::Print(std::ostream& os) const
    // {
    //   os  << "Generalized Local Search Solver: " << this->name << std::endl;
    
    //   if (this->runners.size() > 0)
    //     for (unsigned int i = 0; i < this->runners.size(); i++)
    //       {
    // 	os  << "Runner[" << i << "]" << std::endl;
    // 	this->runners[i]->Print(os);
    //       }
    //   else
    //     os  << "<no runner attached>" << std::endl;
    //   if (p_kicker)
    //     p_kicker->Print(os);
    //   else
    //     os  << "<no kicker attached>" << std::endl;
    //   os << "Max idle rounds: " << max_idle_rounds << std::endl;
    //   os << "Timeout " << this->timeout << std::endl;
    // }
    
    /**
     Solves using a single runner
     */
    template <class Input, class Output, class State, class CostStructure>
    void GRASP<Input, Output, State, CostStructure>::Go(const Input& in)
    {
      unsigned t;
      if (runner == nullptr)
        throw std::logic_error("No runner set for solver " + this->name);
      for (t = 0; t < restarts; t++)
      {
        this->sm.GreedyState(in, this->current_state, alpha, k);
        runner->SetState(in, this->current_state);
        
        this->current_state = runner->GetState();
        this->current_state_cost = runner->GetStateCost();
        
        if (t == 0 || LessThan(this->current_state_cost, this->best_state_cost))
        {
          this->best_state = this->current_state;
          this->best_state_cost = this->current_state_cost;
          if (this->sm.LowerBoundReached(in, this->best_state_cost))
            break;
        }
      }
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void GRASP<Input, Output, State, CostStructure>::AtTimeoutExpired()
    {
      runner->Interrupt();
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void GRASP<Input, Output, State, CostStructure>::ReadParameters(std::istream &is, std::ostream &os)
    {
      os << "GRASP Solver: " << this->name << " parameters" << std::endl;
      os << "Runner: " << std::endl;
      
      this->runner->ReadParameters(is, os);
      
#if defined(HAVE_PTHREAD)
      double timeout;
      os << "Timeout: ";
      is >> timeout;
      this->SetTimeout(timeout);
#endif
    }
    
  } // namespace Core
} // namespace EasyLocal
