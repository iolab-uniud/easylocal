#pragma once

#include <tuple>

#include "helpers/statemanager.hh"
#include "utils/parameter.hh"

namespace EasyLocal
{
  namespace Core
  {
    
    template <class Input, class State, class CostStructure = DefaultCostStructure<int>>
    struct SolverResult
    {
      SolverResult(const State &solution) :  State(solution) {}
      SolverResult(const State &solution, const CostStructure &cost, double running_time) : solution(solution), cost(cost), running_time(running_time) {}
      State solution;
      CostStructure cost;
      double running_time;
    };
    
    /** A Solver represents the external layer of EasyLocal++; it
     implements the Abstract Solver interface and furthermore is
     parametrized with the Input and  State of the problem.
     @ingroup
     Solvers
     */
    template <class _Input, class _State, class _CostStructure = DefaultCostStructure<int>>
    class Solver
    {
    public:
      typedef _Input Input;
      typedef _State State;
      typedef _CostStructure CostStructure;
      
      /** Name of the solver. */
      const std::string name;
      
      
      /** Method to solve the problem.
       @param in an Input object which represents the instance of the problem to solve.
       @return an solver result object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      virtual SolverResult<Input,  State, CostStructure> Solve(const Input& in) = 0;
      
      /** Method to solve a problem again, starting from the a final solution of another run.
       @param initial_solution solution to start with
       @return an  State object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
      virtual SolverResult<Input,  State, CostStructure> Resolve(const Input& in, const  State &initial_solution) = 0;
      
      /** Virtual destructor, for inheritance. */
      virtual ~Solver() {}
      
      /** List of all solvers that have been instantiated so far. For autoloading. */
      static std::vector<Solver<Input,  State, CostStructure>*> solvers;
      
      /** Constructor.
       @param name name of the solver
       */
      Solver(std::string name);
      
      /** Get current solution (meant to be asyncrhonous) */
      virtual std::shared_ptr<State> GetCurrentState(const Input& in) const = 0;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class CostStructure>
    std::vector<Solver<Input,  State, CostStructure> *> Solver<Input,  State, CostStructure>::solvers;
    
    template <class Input, class State, class CostStructure>
    Solver<Input,  State, CostStructure>::Solver(std::string name)
    : name(name)
    {
      solvers.push_back(this);
    }
  } // namespace Core
} // namespace EasyLocal
