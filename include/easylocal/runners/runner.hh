#if !defined(RUNNER_HH_)
#define RUNNER_HH_

#include <stdexcept>
#include <climits>
#include <chrono>
#include <condition_variable>
#include <atomic>

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/utils/interruptible.hh"
#include "easylocal/utils/parameter.hh"

namespace EasyLocal {
  
  namespace Debug {
    
    /** Forward declaration of tester. */
    template <class Input, class State, typename CFtype>
    class AbstractTester;
  }
  
  namespace Core {
    
    
    /** A class representing a single search strategy, e.g. hill climbing
     or simulated annealing. It must be loaded into a solver by using
     Solver::AddRunner() in order to be called correctly.
     @ingroup Helpers
     */
    template <class Input, class State, typename CFtype = int>
    class Runner : public Interruptible<CFtype, State&>, public Parametrized
    {
      friend class Debug::AbstractTester<Input, State, CFtype>;
      
    public:
      
      /** Performs a full run of the search method (possibly being interrupted before its natural ending).
       @param s state to start with and to modify
       @return the cost of the best state found
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      CFtype Go(State& s) throw (ParameterNotSet, IncorrectParameterValue);
      
      /** Performs a given number of steps of the search method on the passed state.
       @param s state to start with and to modify
       @param n the number of steps to make
       @return the cost of the best state found
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      CFtype Step(State& s, unsigned int n = 1) throw (ParameterNotSet, IncorrectParameterValue);
      
      /** Computes the duration of the last run (in milliseconds).
       @return the duration of the last run
       */
      virtual std::chrono::milliseconds GetTimeElapsed() const
      {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - begin);
      }
      
      /** Reads the parameter from an input stream.
       @param is input stream to read from
       @param os output stream to give indication about the needed parameters
       */
      virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
      
      /** Prints the state of the runner.
       @param os output stream to print onto
       */
      virtual void Print(std::ostream& os = std::cout) const;
      
      /** Gets the index of the iteration at which the last best state was found. */
      unsigned int IterationOfBest() const
      {
        return iteration_of_best;
      }
      
      /** Gets the index of the current iteration. */
      unsigned long int Iteration() const
      {
        return iteration;
      }
      
      /** Name of the runner. */
      const std::string name;
      
      /** Description of the runner. */
      const std::string description;
      
      /** Destructor, for inheritance. */
      virtual ~Runner() {}
      
      /** Modality of this runner. */
      virtual unsigned int Modality() const = 0;
      
      /** List of all runners that have been instantiated so far. For autoloading. */
      static std::vector<Runner<Input, State, CFtype>*> runners;
      
    protected:
      
      /** Constructor.
       @param i a reference to the input
       @param sm a StateManager, as defined by the user
       @param name name of the runner
       @param desc description of the runner
       */
      Runner(const Input& i, StateManager<Input, State, CFtype>&, std::string, std::string);
      
      /** Actions and checks to be perfomed at the beginning of the run. Redefinition intended.
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      virtual void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue) {}
      
      /** Actions to be performed at the end of the run. */
      virtual void TerminateRun() = 0;
      
      /** Actions to be performed at each iteration independently of the acceptance of the move */
      virtual void PrepareIteration();
      
      /** Actions to be performed at the end of each iteration independently of the acceptance of the move */
      virtual void CompleteIteration();
      
      /** Encodes the criterion used to stop the search. */
      virtual bool StopCriterion() = 0;
      
      /** Tells if the runner has found a lower bound. For stopping condition. */
      virtual bool LowerBoundReached() const;
      
      /** Tells if the maximum number of iterations allowed have been exhausted. */
      bool MaxIterationExpired() const;
      
      /** Encodes the criterion used to select the move at each step. */
      virtual void SelectMove() = 0;
      
      /** Verifies whether the current move is acceptable (i.e., it is valid) */
      virtual bool AcceptableMoveFound() = 0;
      
      /** Actions to be performed after a move has been accepted. Redefinition intended. */
      virtual void PrepareMove() {};
      
      /** Actually performs the move. */
      virtual void MakeMove() = 0;
      
      /** Actions to be performed after a move has been done. Redefinition intended. */
      virtual void CompleteMove() {};
      
      /** Implements Interruptible. */
      virtual std::function<CFtype(State&)> MakeFunction()
      {
        return [this](State& s) -> CFtype {
          return this->Go(s);
        };
      }
      
      /** No acceptable move has been found in the current iteration. */
      bool no_acceptable_move_found;
      
      /** A reference to the input. */
      const Input& in;
      
      /** The state manager attached to this runner. */
      StateManager<Input, State, CFtype>& sm;
      
      /** Current state of the search. */
      std::shared_ptr<State> p_current_state,
      /** Best state found so far. */
      p_best_state;
      
      /** Cost of the current state. */
      CostComponents<CFtype> current_state_cost;
      
      /** Cost of the best state. */
      CostComponents<CFtype> best_state_cost;
      
      /** Index of the iteration where the best has been found. */
      unsigned long int iteration_of_best;
      
      /** Index of the current iteration. */
      unsigned long int iteration;
      
      /** Number of cost function evaluations. */
      unsigned long int evaluations;
      
      /** Generic parameter of every runner: maximum number of iterations. */
      Parameter<unsigned long int> max_iterations;
      
      /** Chronometer. */
      std::chrono::high_resolution_clock::time_point begin, end;
      
    private:
      
      /** Stores the move and updates the related data. */
      virtual void UpdateBestState() = 0;
      
      /** Actions that must be done at the start of the search, and which cannot be redefined by subclasses. */
      void InitializeRun(State&) throw (ParameterNotSet, IncorrectParameterValue);
      
      /** Actions that must be done at the end of the search. */
      CFtype TerminateRun(State&);
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, typename CFtype>
    std::vector<Runner<Input, State, CFtype>*> Runner<Input, State, CFtype>::runners;
    
    template <class Input, class State, typename CFtype>
    Runner<Input, State, CFtype>::Runner(const Input& i, StateManager<Input, State, CFtype>& sm, std::string name, std::string description)
    : // Parameters
    Parametrized(name, description), name(name), description(description), no_acceptable_move_found(false), in(i), sm(sm),
    max_iterations("max_iterations", "Maximum total number of iterations allowed", this->parameters)
    {
      // This parameter has a default value
      max_iterations = std::numeric_limits<unsigned long int>::max();
      
      // Add to the list of all runners
      runners.push_back(this);
      begin = end = std::chrono::high_resolution_clock::now();
    }
    
    template <class Input, class State, typename CFtype>
    CFtype Runner<Input, State, CFtype>::Go(State& s) throw (ParameterNotSet, IncorrectParameterValue)
    {
      InitializeRun(s);
      while (!MaxIterationExpired() && !StopCriterion() && !LowerBoundReached() && !this->TimeoutExpired())
      {
        PrepareIteration();
        try
        {
          SelectMove();
          if (AcceptableMoveFound())
          {
            PrepareMove();
            MakeMove();
            CompleteMove();
            UpdateBestState();
          }
        }
        catch (EmptyNeighborhood)
        {
          break;
        }
        CompleteIteration();
      }
      
      return TerminateRun(s);
    }
    
    
    /**
     Prepare the iteration (e.g. updates the counter that tracks the number of iterations elapsed)
     */
    template <class Input, class State, typename CFtype>
    void Runner<Input, State, CFtype>::PrepareIteration()
    {
      no_acceptable_move_found = false;
      iteration++;
    }
    
    /**
     Complete the iteration (e.g. decreate the temperature for Simulated Annealing)
     */
    template <class Input, class State, typename CFtype>
    void Runner<Input, State, CFtype>::CompleteIteration()
    {
      end = std::chrono::high_resolution_clock::now();
    }
    
    template <class Input, class State, typename CFtype>
    bool Runner<Input, State, CFtype>::MaxIterationExpired() const
    {
      return iteration >= max_iterations;
    }
    
    template <class Input, class State, typename CFtype>
    void Runner<Input, State, CFtype>::InitializeRun(State& s) throw (ParameterNotSet, IncorrectParameterValue)
    {
      begin = std::chrono::high_resolution_clock::now();
      iteration = 0;
      iteration_of_best = 0;
      evaluations = 0;
      p_best_state = std::make_shared<State>(s);    // creates the best state object by copying the content of s
      p_current_state = std::make_shared<State>(s); // creates the current state object by copying the content of s
      best_state_cost = current_state_cost = sm.CostFunctionComponents(s);
      InitializeRun();
      end = std::chrono::high_resolution_clock::now();
    }
    
    template <class Input, class State, typename CFtype>
    CFtype Runner<Input, State, CFtype>::TerminateRun(State& s)
    {
      s = *p_best_state;
      TerminateRun();
      end = std::chrono::high_resolution_clock::now();
      return best_state_cost.total;
    }
    
    template <class Input, class State, typename CFtype>
    bool Runner<Input, State, CFtype>::LowerBoundReached() const
    {
      return sm.LowerBoundReached(current_state_cost.total);
    }
    
    template <class Input, class State, typename CFtype>
    void Runner<Input, State, CFtype>::ReadParameters(std::istream& is, std::ostream& os)
    {
      os << this->name << " -- INPUT PARAMETERS" << std::endl;
      Parametrized::ReadParameters();
    }
    
    template <class Input, class State, typename CFtype>
    void Runner<Input, State, CFtype>::Print(std::ostream& os) const
    {
      os  << "  " << this->name << std::endl;
      Parametrized::Print(os);  
    }    
  }
}

#endif // _RUNNER_HH_
