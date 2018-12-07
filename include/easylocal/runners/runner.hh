#pragma once

#include <stdexcept>
#include <climits>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <typeinfo>
#include <functional>

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/utils/interruptible.hh"
#include "easylocal/utils/parameter.hh"
#include "easylocal/helpers/coststructure.hh"

namespace EasyLocal
{
  namespace Debug
  {    
    /** Forward declaration of tester. */
    template <class Input, class State, class CostStructure>
    class AbstractTester;
  } // namespace Debug
  
  namespace Core
  {
    
    /** A class representing a single search strategy, e.g. hill climbing
     or simulated annealing. It must be loaded into a solver by using
     Solver::AddRunner() in order to be called correctly.
     @ingroup Helpers
     */
    template <class _Input, class _State, class _CostStructure = DefaultCostStructure<int>>
    class Runner : public Interruptible<_CostStructure, const _Input&, _State &>, public CommandLineParameters::Parametrized
    {
      friend class Debug::AbstractTester<_Input, _State, _CostStructure>;
      
    public:
      typedef _Input Input;
      typedef _State State;
      typedef _CostStructure CostStructure;
      typedef typename _CostStructure::CFtype CFtype;
      
      
      /** Performs a full run of the search method (possibly being interrupted before its natural ending).
       @param in the input object
       @param s state to start with and to modify
       @return the cost of the best state found
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      CostStructure Go(const Input& in, State &s);
      
      
      /** Performs a given number of steps of the search method on the passed state.
       @param s state to start with and to modify
       @param n the number of steps to make
       @return the cost of the best state found
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      CostStructure Step(State &s, unsigned int n = 1);
      
      /** Register its parameters */
      virtual void InitializeParameters();
      
      /** Reads the parameter from an input stream.
       @param is input stream to read from
       @param os output stream to give indication about the needed parameters
       */
      virtual void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout);
      
      /** Prints the state of the runner.
       @param os output stream to print onto
       */
      virtual void Print(std::ostream &os = std::cout) const;
      
      /** Gets the index of the iteration at which the last best state was found. */
      unsigned int IterationOfBest() const
      {
        return iteration_of_best;
      }
      
      /** Gets the ... */
      unsigned long int MaxEvaluations() const
      {
        return max_evaluations;
      }
      
      /** Set the ... */
      void SetMaxEvaluations(unsigned long int me)
      {
        max_evaluations = me;
      }
      
      /** Gets the index of the current iteration. */
      unsigned long int Iteration() const
      {
        return iteration;
      }
      
      /** Name of the runner. */
      const std::string name;
      
      /** Destructor, just for inheritance. */
      virtual ~Runner()
      {}
      
      /** Modality of this runner. */
      virtual size_t Modality() const = 0;
      
      /** List of all runners that have been instantiated so far: for autoloading.
       It does not include the cloned runners.
       */
      static std::vector<Runner<Input, State, CostStructure> *> runners;
      
      virtual std::shared_ptr<State> GetCurrentBestState() const;
      
      virtual CostStructure GetCurrentBestCost() const;
      
      virtual std::unique_ptr<Runner<Input, State, CostStructure>> Clone() const = 0;
      
      void PrepareParameters(const Parametrized& p)
      {
        this->InitializeParameters();
        this->CopyParameterValues(p);
      }
      
    protected:
      
      /** Constructor.
       @param sm a StateManager, as defined by the user
       @param name the name of the runner
       */
      Runner(StateManager<Input, State, CostStructure> &sm, std::string name, std::string description);
      
      /** Copy Constructor.
       @param r a Runner to be copied
       */
      Runner(const Runner<Input, State, CostStructure>& r);
      
      /** Actions and checks to be perfomed at the beginning of the run. Redefinition intended.
       @param in the Input object
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      virtual void InitializeRun(const Input& in) {}
      
      /** Actions to be performed at the end of the run.
       @param in the Input object
       */
      virtual void TerminateRun(const Input& in) {};
      
      /** Actions to be performed at each iteration independently of the acceptance of the move */
      virtual void PrepareIteration(const Input& in);
      
      /** Actions to be performed at the end of each iteration independently of the acceptance of the move */
      virtual void CompleteIteration(const Input& in);
      
      /** Encodes the criterion used to stop the search. */
      virtual bool StopCriterion() const = 0;
      
      /** Tells if the runner has found a lower bound. For stopping condition. */
      virtual bool LowerBoundReached(const Input& in) const;
      
      /** Tells if the maximum number of cot function evaluations allowed have been exhausted. */
      bool MaxEvaluationsExpired() const;
      
      /** Encodes the criterion used to select the move at each step. */
      virtual void SelectMove(const Input& in) = 0;
      
      /** Verifies whether the current move is acceptable (i.e., it is valid) */
      virtual bool AcceptableMoveFound(const Input& in) = 0;
      
      /** Actions to be performed after a move has been accepted. Redefinition intended. */
      virtual void PrepareMove(const Input& in) {};
      
      /** Actually performs the move. */
      virtual void MakeMove(const Input& in) = 0;
      
      /** Actions to be performed after a move has been done. Redefinition intended. */
      virtual void CompleteMove(const Input& in) {};
      
      /** Implements Interruptible. */
      virtual std::function<CostStructure(const Input&, State&)> MakeFunction()
      {
        return [this](const Input& in, State &st) -> CostStructure { return this->Go(in, st); };
      }
      
      /** No acceptable move has been found in the current iteration. */
      bool no_acceptable_move_found;
      
      /** The state manager attached to this runner. */
      StateManager<Input, State, CostStructure> &sm;
      
      // TODO: probably unique_ptr are more suitable for this case
      /** Current state of the search. */
      std::shared_ptr<State> p_current_state,
      /** Best state found so far. */
      p_best_state;
      
      mutable std::mutex best_state_mutex, go_mutex;
      
      /** Cost of the current state. */
      CostStructure current_state_cost;
      
      /** Cost of the best state. */
      CostStructure best_state_cost;
      
      /** Index of the iteration where the best has been found. */
      unsigned long int iteration_of_best;
      
      /** Index of the current iteration. */
      unsigned long int iteration;
      
      /** Number of cost function evaluations. */
      unsigned long int evaluations;
      
      /** Generic parameter of every runner: maximum number of cost function evaluations. */
      Parameter<unsigned long int> max_evaluations;
      
      /** Weights of the different cost function components.
       If the vector is empty (default), it is assumed all weigths to be 1.0. */
      std::vector<double> weights;
      
      template <typename ActualRunnerType>
      static std::unique_ptr<Runner<Input, State, CostStructure>> MakeClone(const ActualRunnerType* r)
      {
        std::unique_ptr<ActualRunnerType> new_r = std::make_unique<ActualRunnerType>(*r);
        new_r->PrepareParameters(*r);
        return new_r;
      }
      
    private:
      /** Stores the move and updates the related data. */
      virtual void UpdateBestState() = 0;
      
      /** Actions that must be done at the start of the search, and which cannot be redefined by subclasses. */
      void InitializeRun(const Input& in, State& st);
      
      /** Actions that must be done at the end of the search. */
      CostStructure TerminateRun(const Input& in, State& st);
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class CostStructure>
    std::vector<Runner<Input, State, CostStructure> *> Runner<Input, State, CostStructure>::runners;
    
    
    template <class Input, class State, class CostStructure>
    Runner<Input, State, CostStructure>::Runner(StateManager<Input, State, CostStructure> &sm, std::string name, std::string description)
    : // Parameters
    Parametrized(name, description), name(name), no_acceptable_move_found(false), sm(sm), weights(0)
    {
      // Add to the list of all runners
      runners.push_back(this);
    }
    
    template <class Input, class State, class CostStructure>
    Runner<Input, State, CostStructure>::Runner(const Runner<Input, State, CostStructure>& r)
    : // Parameters
    Parametrized(r.name, "Copy of " + r.name), name(r.name), no_acceptable_move_found(r.no_acceptable_move_found), sm(r.sm), weights(r.weights)
    {}
    
    template <class Input, class State, class CostStructure>
    void Runner<Input, State, CostStructure>::InitializeParameters()
    {
      max_evaluations("max_evaluations", "Maximum total number of cost function evaluations allowed", this->parameters);
      // This parameter has a default value
      max_evaluations = std::numeric_limits<unsigned long int>::max();
    }
    
    template <class Input, class State, class CostStructure>
    CostStructure Runner<Input, State, CostStructure>::Go(const Input& in, State &st)
    {
      std::lock_guard<std::mutex> lock(go_mutex);
      InitializeRun(in, st);
      while (!MaxEvaluationsExpired() && !StopCriterion() && !LowerBoundReached(in) && !this->TimeoutExpired() && !this->Aborted())
      {
        PrepareIteration(in);
        try
        {
          SelectMove(in);
          if (AcceptableMoveFound(in))
          {
            PrepareMove(in);
            MakeMove(in);
            CompleteMove(in);
            UpdateBestState();
          }
        }
        catch (EmptyNeighborhood)
        {
          break;
        }
        CompleteIteration(in);
      }
      
      return TerminateRun(in, st);
    }
    
    /**
     Prepare the iteration (e.g. updates the counter that tracks the number of iterations elapsed)
     */
    template <class Input, class State, class CostStructure>
    void Runner<Input, State, CostStructure>::PrepareIteration(const Input& in)
    {
      no_acceptable_move_found = false;
      iteration++;
    }
    
    /**
     Complete the iteration (e.g. decreate the temperature for Simulated Annealing)
     */
    template <class Input, class State, class CostStructure>
    void Runner<Input, State, CostStructure>::CompleteIteration(const Input&)
    {
    }
    
    template <class Input, class State, class CostStructure>
    void Runner<Input, State, CostStructure>::InitializeRun(const Input& in, State &st)
    {
      iteration = 0;
      iteration_of_best = 0;
      evaluations = 0;
      p_best_state = std::make_shared<State>(st);    // creates the best state object by copying the content of s
      p_current_state = std::make_shared<State>(st); // creates the current state object by copying the content of s
      best_state_cost = current_state_cost = sm.CostFunctionComponents(in, st);
      InitializeRun(in);
    }
    
    template <class Input, class State, class CostStructure>
    CostStructure Runner<Input, State, CostStructure>::TerminateRun(const Input& in, State &st)
    {
      st = *p_best_state;
      TerminateRun(in);
      return best_state_cost;
    }
    
    template <class Input, class State, class CostStructure>
    bool Runner<Input, State, CostStructure>::LowerBoundReached(const Input& in) const
    {
      return sm.LowerBoundReached(in, current_state_cost);
    }
    
    template <class Input, class State, class CostStructure>
    bool Runner<Input, State, CostStructure>::MaxEvaluationsExpired() const
    {
      return evaluations >= max_evaluations;
    }
    
    template <class Input, class State, class CostStructure>
    void Runner<Input, State, CostStructure>::ReadParameters(std::istream &is, std::ostream &os)
    {
      os << this->name << " -- INPUT PARAMETERS" << std::endl;
      Parametrized::ReadParameters(is, os);
    }
    
    template <class Input, class State, class CostStructure>
    void Runner<Input, State, CostStructure>::Print(std::ostream &os) const
    {
      os << "  " << this->name << std::endl;
      Parametrized::Print(os);
    }
    
    template <class Input, class State, class CostStructure>
    std::shared_ptr<State> Runner<Input, State, CostStructure>::GetCurrentBestState() const
    {
      std::lock_guard<std::mutex> lock(best_state_mutex);
      return std::make_shared<State>(*p_best_state); // make a state copy
    }
    
    template <class Input, class State, class CostStructure>
    CostStructure Runner<Input, State, CostStructure>::GetCurrentBestCost() const
    {
      std::lock_guard<std::mutex> lock(best_state_mutex);
      return best_state_cost; // make a state copy
    }
  } // namespace Core
} // namespace EasyLocal
