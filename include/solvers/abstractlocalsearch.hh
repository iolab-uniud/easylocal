#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <typeinfo>
#include <atomic>

#include "solvers/solver.hh"
#include "helpers/statemanager.hh"
#include "helpers/outputmanager.hh"
#include "runners/runner.hh"
#include "utils/parameter.hh"
#include "utils/interruptible.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** A Local Search Solver has an internal state, and defines the ways for
     dealing with a local search algorithm.
     @ingroup Solvers
     */
    template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
    class AbstractLocalSearch
    : public CommandLineParameters::Parametrized,
    public Solver<Input, Output, CostStructure>,
    public Interruptible<bool, const Input&>
    {
    public:
      typedef Interruptible<bool, const Input&> InterruptibleType;
      
      
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      SolverResult<Input, Output, CostStructure> Solve()
      {
        return Solver<Input, Output, CostStructure>::Solve(this->GetInput());
      }      
      /** @copydoc Solver */
      virtual SolverResult<Input, Output, CostStructure> Solve(const Input& in) final;
      /** @copydoc Solver */
      virtual SolverResult<Input, Output, CostStructure> Resolve(const Input& in, const Output &initial_solution) final;
      
      /** Constructor.
       @param in the Input object
       @param sm a StateManager
       @param om an OutputManager
       @param name a name for the solver
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      AbstractLocalSearch(const Input &in,
                          StateManager<Input, State, CostStructure> &sm,
                          OutputManager<Input, Output, State> &om,
                          std::string name);
      
      /** Constructor.
       @param sm a StateManager
       @param om an OutputManager
       @param name a name for the solver
       */
      AbstractLocalSearch(StateManager<Input, State, CostStructure> &sm,
                          OutputManager<Input, Output, State> &om,
                          std::string name);
      
      // FIXME: to review
      //[[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      virtual std::shared_ptr<Output> GetCurrentSolution() const;
      
    protected:
      // FIXME: to review
      //[[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      virtual std::shared_ptr<State> GetCurrentState() const = 0;
      
      virtual ~AbstractLocalSearch()
      {}
      
      /** Implements Interruptible. */
      virtual std::function<bool(const Input& in)> MakeFunction()
      {
        return [this](const Input& in) -> bool {
          this->ResetTimeout();
          this->Go(in);
          return true;
        };
      }
      
      virtual void TerminateSolve();
      virtual void FindInitialState(const Input& in);
      // This will be the actual solver strategy implementation
      virtual void Go(const Input& in) = 0;
      StateManager<Input, State, CostStructure> &sm;        /**< A pointer to the attached
                                                             state manager. */
      OutputManager<Input, Output, State> &om;              /**< A pointer to the attached
                                                             output manager. */
      std::shared_ptr<State> p_current_state, p_best_state; /**< The internal states of the solver. */
      
      CostStructure current_state_cost, best_state_cost; /**< The cost of the internal states. */
      std::shared_ptr<Output> p_out;                     /**< The output object of the solver. */
      // parameters
      
      void InitializeParameters();
      
      Parameter<unsigned int> init_trials;
      Parameter<bool> random_initial_state;
      Parameter<double> timeout;
      std::atomic<bool> is_running;
      
    private:
      void InitializeSolve(const Input& in);
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     @brief Constructs an abstract local search solver.
     
     @param in an input object
     @param e_sm a compatible state manager
     @param e_om a compatible output manager
     @param name a descriptive name for the solver
     */
    template <class Input, class Output, class State, class CostStructure>
    AbstractLocalSearch<Input, Output, State, CostStructure>::AbstractLocalSearch(const Input &in,
                                                                                  StateManager<Input, State, CostStructure> &sm,
                                                                                  OutputManager<Input, Output, State> &om,
                                                                                  std::string name)
    : CommandLineParameters::Parametrized(name, typeid(this).name()),
    Solver<Input, Output, CostStructure>(in, name),
    sm(sm),
    om(om),
    is_running(false)
    {}
    
    template <class Input, class Output, class State, class CostStructure>
    AbstractLocalSearch<Input, Output, State, CostStructure>::AbstractLocalSearch(StateManager<Input, State, CostStructure> &sm,
                                                                                  OutputManager<Input, Output, State> &om,
                                                                                  std::string name)
    : CommandLineParameters::Parametrized(name, typeid(this).name()),
    Solver<Input, Output, CostStructure>(name),
    sm(sm),
    om(om),
    is_running(false)
    {}
    
    template <class Input, class Output, class State, class CostStructure>
    void AbstractLocalSearch<Input, Output, State, CostStructure>::InitializeParameters()
    {
      init_trials("init_trials", "Number of states to be tried in the initialization phase", this->parameters);
      random_initial_state("random_state", "Random initial state", this->parameters);
      timeout("timeout", "Solver timeout (if not specified, no timeout)", this->parameters);
      init_trials = 1;
      random_initial_state = true;
    }
    
    /**
     The initial state is generated by delegating this task to
     the state manager. The function invokes the SampleState function.
     */
    template <class Input, class Output, class State, class CostStructure>
    void AbstractLocalSearch<Input, Output, State, CostStructure>::FindInitialState(const Input& in)
    {
      if (random_initial_state)
        current_state_cost = sm.SampleState(in, *p_current_state, init_trials);
      else
      {
        sm.GreedyState(in, *p_current_state);
        current_state_cost = sm.CostFunctionComponents(in, *p_current_state);
      }
      *p_best_state = *p_current_state;
      best_state_cost = current_state_cost;
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void AbstractLocalSearch<Input, Output, State, CostStructure>::InitializeSolve(const Input& in)
    {
      p_best_state = std::make_shared<State>(in);
      p_current_state = std::make_shared<State>(in);
    }
    
    template <class Input, class Output, class State, class CostStructure>
    SolverResult<Input, Output, CostStructure> AbstractLocalSearch<Input, Output, State, CostStructure>::Solve(const Input& in)
    {
      auto start = std::chrono::high_resolution_clock::now();
      is_running = true;
      InitializeSolve(in);
      FindInitialState(in);
      if (timeout.IsSet())
        this->SyncRun(std::chrono::milliseconds(static_cast<long long int>(timeout * 1000.0)), in);
      else
        Go(in);
      p_out = std::make_shared<Output>(in);
      om.OutputState(in, *p_best_state, *p_out);
      TerminateSolve();
      
      double run_time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::high_resolution_clock::now() - start).count();
      is_running = false;
      
      return SolverResult<Input, Output, CostStructure>(*p_out, sm.CostFunctionComponents(in, *p_best_state), run_time);
    }
    
    template <class Input, class Output, class State, class CostStructure>
    SolverResult<Input, Output, CostStructure> AbstractLocalSearch<Input, Output, State, CostStructure>::Resolve(const Input& in, const Output &initial_solution)
    {
      auto start = std::chrono::high_resolution_clock::now();
      is_running = true;
      
      InitializeSolve(in);
      om.InputState(in, *p_current_state, initial_solution);
      *p_best_state = *p_current_state;
      best_state_cost = current_state_cost = sm.CostFunctionComponents(in, *p_current_state);
      if (timeout.IsSet())
        this->SyncRun(std::chrono::milliseconds(static_cast<long long int>(timeout * 1000.0)), in);
      else
        Go(in);
      p_out = std::make_shared<Output>(in);
      om.OutputState(in, *p_best_state, *p_out);
      TerminateSolve();
      is_running = false;
      
      double run_time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::high_resolution_clock::now() - start).count();
      
      return SolverResult<Input, Output, CostStructure>(*p_out, sm.CostFunctionComponents(in, *p_best_state), run_time);
    }
    
    template <class Input, class Output, class State, class CostStructure>
    void AbstractLocalSearch<Input, Output, State, CostStructure>::TerminateSolve()
    {}
    
    // FIXME: currently does not have the input
    template <class Input, class Output, class State, class CostStructure>
    std::shared_ptr<Output> AbstractLocalSearch<Input, Output, State, CostStructure>::GetCurrentSolution() const
    {
      std::shared_ptr<State> current_state;
      if (!is_running)
        current_state = this->p_best_state;
      else
        current_state = GetCurrentState();
      std::shared_ptr<Output> out = std::make_shared<Output>(this->GetInput());
      om.OutputState(this->GetInput(), *current_state, *out);
      
      return out;
    }
  } // namespace Core
} // namespace EasyLocal
