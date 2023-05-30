#pragma once

#include <stdexcept>
#include <climits>
#include <chrono>
#include <condition_variable>
#include <atomic>
#include <typeinfo>

#include "helpers/solutionmanager.hh"
#include "helpers/neighborhoodexplorer.hh"
#include "utils/interruptible.hh"
#include "utils/parameter.hh"
#include "helpers/coststructure.hh"

namespace EasyLocal
{

namespace Debug
{

/** Forward declaration of tester. */
template <class Input, class Solution, class CostStructure>
class AbstractTester;
} // namespace Debug

namespace Core
{

/** A class representing a single search strategy, e.g. hill climbing
     or simulated annealing. It must be loaded into a solver by using
     Solver::AddRunner() in order to be called correctly.
     @ingroup Helpers
     */
template <class Input, class Solution, class CostStructure = DefaultCostStructure<int>>
  class Runner :
public Interruptible<CostStructure, Solution&>,
public CommandLineParameters::Parametrized
{
  friend class Debug::AbstractTester<Input, Solution, CostStructure>;

public:
  typedef Input InputType;
  typedef Solution SolutionType;
  typedef typename CostStructure::CFtype CFtype;
  typedef CostStructure CostStructureType;

  /** Performs a full run of the search method (possibly being interrupted before its natural ending).
       @param s state to start with and to modify
       @return the cost of the best state found
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
  CostStructure Go(Solution&s);

  /** Performs a given number of steps of the search method on the passed state.
       @param s state to start with and to modify
       @param n the number of steps to make
       @return the cost of the best state found
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
  CostStructure Step(Solution&s, unsigned int n = 1);

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
 
  /** Gets the index of the current evaluation. */
  unsigned long int Evaluations() const
  {
    return evaluations;
  }

  /** Name of the runner. */
  const std::string name;

  /** Destructor, for inheritance. */
  virtual ~Runner() {}

  /** Modality of this runner. */
  virtual size_t Modality() const = 0;

  /** List of all runners that have been instantiated so far. For autoloading. */
  static std::vector<Runner<Input, Solution, CostStructure> *> runners;

  virtual std::shared_ptr<Solution> GetCurrentBestState() const;

protected:
  /** Constructor.
       @param i a reference to the input
       @param sm a SolutionManager, as defined by the user
       @param name name of the runner
       @param desc description of the runner
       */
  Runner(const Input &, SolutionManager<Input, Solution, CostStructure> &, std::string);
  
  Runner(const Runner<Input, Solution, CostStructure>& r) {}

  /** Actions and checks to be perfomed at the beginning of the run. Redefinition intended.
       @throw ParameterNotSet if one of the parameters needed by the runner (or other components) hasn't been set
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
  virtual void InitializeRun() {}

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

  /** Tells if the maximum number of cot function evaluations allowed have been exhausted. */
  virtual bool MaxEvaluationsExpired() const;

  /** Encodes the criterion used to select the move at each step. */
  virtual void SelectMove() = 0;

  /** Verifies whether the current move is acceptable (i.e., it is valid) */
  virtual bool AcceptableMoveFound() = 0;

  /** Actions to be performed after a move has been accepted. Redefinition intended. */
  virtual void PrepareMove(){};

  /** Actually performs the move. */
  virtual void MakeMove() = 0;

  /** Actions to be performed after a move has been done. Redefinition intended. */
  virtual void CompleteMove(){};

  /** Implements Interruptible. */
  virtual std::function<CostStructure(Solution&)> MakeFunction()
  {
    return [this](Solution&s) -> CostStructure { return this->Go(s); };
  }

  /** No acceptable move has been found in the current iteration. */
  bool no_acceptable_move_found;

  /** A reference to the input. */
  const Input &in;

  /** The state manager attached to this runner. */
  SolutionManager<Input, Solution, CostStructure> &sm;

  /** Current state of the search. */
  std::shared_ptr<Solution> p_current_state,
      /** Best state found so far. */
      p_best_state;

  mutable std::mutex best_state_mutex;

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

private:
  /** Stores the move and updates the related data. */
  virtual void UpdateBestState() = 0;

  /** Actions that must be done at the start of the search, and which cannot be redefined by subclasses. */
  void InitializeRun(Solution&);

  /** Actions that must be done at the end of the search. */
  CostStructure TerminateRun(Solution&);
};

/*************************************************************************
     * Implementation
     *************************************************************************/

template <class Input, class Solution, class CostStructure>
std::vector<Runner<Input, Solution, CostStructure> *> Runner<Input, Solution, CostStructure>::runners;

template <class Input, class Solution, class CostStructure>
Runner<Input, Solution, CostStructure>::Runner(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm, std::string name)
    : // Parameters
  CommandLineParameters::Parametrized(name, typeid(this).name()), name(name), no_acceptable_move_found(false), in(in), sm(sm), weights(0)
{
  // Add to the list of all runners
    runners.push_back(this);
    max_evaluations("max_evaluations", "Maximum total number of cost function evaluations allowed", this->parameters);
    // This parameter has a default value
    //    max_evaluations = std::numeric_limits<unsigned long int>::max();
}

template <class Input, class Solution, class CostStructure>
CostStructure Runner<Input, Solution, CostStructure>::Go(Solution&s)
{
  InitializeRun(s);
  while (!MaxEvaluationsExpired() && !StopCriterion() && !LowerBoundReached() && !this->TimeoutExpired())
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
    catch (EmptyNeighborhood&)
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
template <class Input, class Solution, class CostStructure>
void Runner<Input, Solution, CostStructure>::PrepareIteration()
{
  no_acceptable_move_found = false;
  iteration++;
}

/**
     Complete the iteration (e.g. decreate the temperature for Simulated Annealing)
     */
template <class Input, class Solution, class CostStructure>
void Runner<Input, Solution, CostStructure>::CompleteIteration()
{
}

template <class Input, class Solution, class CostStructure>
void Runner<Input, Solution, CostStructure>::InitializeRun(Solution&s)
{
  iteration = 0;
  iteration_of_best = 0;
  evaluations = 0;
  p_best_state = std::make_shared<Solution>(s);    // creates the best state object by copying the content of s
  p_current_state = std::make_shared<Solution>(s); // creates the current state object by copying the content of s
  best_state_cost = current_state_cost = sm.CostFunctionComponents(s);
  InitializeRun();
}

template <class Input, class Solution, class CostStructure>
CostStructure Runner<Input, Solution, CostStructure>::TerminateRun(Solution&s)
{
  s = *p_best_state;
  TerminateRun();
  return best_state_cost;
}

template <class Input, class Solution, class CostStructure>
bool Runner<Input, Solution, CostStructure>::LowerBoundReached() const
{
  return sm.LowerBoundReached(current_state_cost);
}

template <class Input, class Solution, class CostStructure>
bool Runner<Input, Solution, CostStructure>::MaxEvaluationsExpired() const
{
  return evaluations >= max_evaluations;
}

template <class Input, class Solution, class CostStructure>
void Runner<Input, Solution, CostStructure>::ReadParameters(std::istream &is, std::ostream &os)
{
  os << this->name << " -- INPUT PARAMETERS" << std::endl;
  CommandLineParameters::Parametrized::ReadParameters(is, os);
}

template <class Input, class Solution, class CostStructure>
void Runner<Input, Solution, CostStructure>::Print(std::ostream &os) const
{
  os << "  " << this->name << std::endl;
  CommandLineParameters::Parametrized::Print(os);
}

template <class Input, class Solution, class CostStructure>
std::shared_ptr<Solution> Runner<Input, Solution, CostStructure>::GetCurrentBestState() const
{
  std::lock_guard<std::mutex> lock(best_state_mutex);
  return std::make_shared<Solution>(*p_best_state); // make a state copy
}
} // namespace Core
} // namespace EasyLocal
