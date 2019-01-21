#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <typeinfo>
#include <atomic>

#include "solvers/solver.hh"
#include "helpers/statemanager.hh"
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
    template <class Input, class State, class CostStructure>
    class AbstractLocalSearch
    : public CommandLineParameters::Parametrized,
    public Solver<Input, State, CostStructure>,
    public Interruptible<bool, const Input&>
    {
    public:
      using InterruptibleType = Interruptible<bool, const Input&>;
      using Result = typename Solver<Input, State, CostStructure>::Result;
      
      /** @copydoc Solver */
      virtual Result Solve(const Input& in) override final
      {
        std::lock_guard<std::mutex> lock(solve_mutex);
        auto start = std::chrono::high_resolution_clock::now();
        is_running = true;
        InitializeSolve(in);
        FindInitialState(in);
        if (timeout.IsSet())
          this->SyncRun(std::chrono::milliseconds(static_cast<long long int>(timeout * 1000.0)), in);
          else
            Go(in);
            TerminateSolve();
            
            double run_time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::high_resolution_clock::now() - start).count();
            is_running = false;
            
            return Result(*p_best_state, sm.CostFunctionComponents(in, *p_best_state), run_time);
      }
      
      /** @copydoc Solver */
      virtual Result Resolve(const Input& in, const State &initial_solution) override final
      {
        std::lock_guard<std::mutex> lock(solve_mutex);
        auto start = std::chrono::high_resolution_clock::now();
        is_running = true;
        
        InitializeSolve(in);
        *p_current_state = initial_solution;
        *p_best_state = *p_current_state;
        best_state_cost = current_state_cost = sm.CostFunctionComponents(in, *p_current_state);
        if (timeout.IsSet())
          this->SyncRun(std::chrono::milliseconds(static_cast<long long int>(timeout * 1000.0)), in);
          else
            Go(in);
            TerminateSolve();
            is_running = false;
            
            double run_time = std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1>>>(std::chrono::high_resolution_clock::now() - start).count();
            
            return Result(*p_best_state, sm.CostFunctionComponents(in, *p_best_state), run_time);
      }
      
      /** Constructor.
       @param sm a StateManager
       @param name a name for the solver
       */
      AbstractLocalSearch(StateManager<Input, State, CostStructure> &sm,
                          const std::string& name)
      : Parametrized(name, typeid(this).name()),
      Solver<Input, State, CostStructure>(name),
      sm(sm),
      is_running(false)
      {}
      
      virtual std::shared_ptr<State> GetCurrentState(const Input& in) const override
      {
        std::shared_ptr<State> current_state;
        if (!is_running)
          current_state = this->p_best_state;
        else
          current_state = GetCurrentState();
        
        return current_state;
      }
    protected:      
      virtual ~AbstractLocalSearch()
      {}
      
      virtual std::shared_ptr<State> GetCurrentState() const = 0;

      
      /** Implements Interruptible. */
      virtual std::function<bool(const Input& in)> MakeFunction() override
      {
        return [this](const Input& in) -> bool {
          this->ResetTimeout();
          this->Go(in);
          return true;
        };
      }
      
      virtual void TerminateSolve()
      {}
      
      virtual void FindInitialState(const Input& in)
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
      
      // This will be the actual solver strategy implementation
      virtual void Go(const Input& in) = 0;
      StateManager<Input, State, CostStructure> &sm;        /**< A reference to the attached
                                                             state manager. */
      std::shared_ptr<State> p_current_state, p_best_state; /**< The internal states of the solver. */
      
      CostStructure current_state_cost, best_state_cost; /**< The cost of the internal states. */
      // parameters
      
      void InitializeParameters() override
      {
        init_trials("init_trials", "Number of states to be tried in the initialization phase", this->parameters);
        random_initial_state("random_state", "Random initial state", this->parameters);
        timeout("timeout", "Solver timeout (if not specified, no timeout)", this->parameters);
        init_trials = 1;
        random_initial_state = true;
      }
      
      Parameter<unsigned int> init_trials;
      Parameter<bool> random_initial_state;
      Parameter<double> timeout;
      std::atomic<bool> is_running;
      
      std::mutex solve_mutex;
      
    private:
      void InitializeSolve(const Input& in)
      {
        p_best_state = std::make_shared<State>(in);
        p_current_state = std::make_shared<State>(in);
      }
    };
  } // namespace Core
} // namespace EasyLocal
