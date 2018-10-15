#pragma once

#include <tuple>

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/outputmanager.hh"
#include "easylocal/utils/parameter.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    template <class Input, class Output, class CostStructure = DefaultCostStructure<int>>
    struct SolverResult
    {
      SolverResult(const Output &output) : output(output) {}
      SolverResult(const Output &output, const CostStructure &cost, double running_time) : output(output), cost(cost), running_time(running_time) {}
      Output output;
      CostStructure cost;
      double running_time;
    };
    
    /** A Solver represents the external layer of EasyLocal++; it
     implements the Abstract Solver interface and furthermore is
     parametrized with the Input and Output of the problem.
     @ingroup
     Solvers
     */
    template <class _Input, class _Output, class _CostStructure = DefaultCostStructure<int>>
    class Solver
    {
    public:
      typedef _Input Input;
      typedef _Output Output;
      typedef _CostStructure CostStructure;
      
      /** Name of the solver. */
      const std::string name;
      
      /** Method to solve the problem which has been set up in the constructor.
       @return an solver result object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       @deprecated
       */
      [[deprecated("This is the old style solver interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      SolverResult<Input, Output, CostStructure> Solve()
      {
        if (!p_in)
          throw std::runtime_error("You are currently mixing the old-style and new-style solver usage. This method could be called only with the old-style usage");
        return Solve(*p_in);
      };
      
      /** Method to solve the problem.
       @param in an Input object which represents the instance of the problem to solve.
       @return an solver result object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       @deprecated
       */
      virtual SolverResult<Input, Output, CostStructure> Solve(const Input& in) = 0;
      
      /** Method to solve a problem again, starting from the a final solution of another run.
       @param initial_solution solution to start with
       @return an output object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       @deprecated
       */
      [[deprecated("This is the old style solver interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      SolverResult<Input, Output, CostStructure> Resolve(const Output &initial_solution)
      {
        if (!p_in)
          throw std::runtime_error("You are currently mixing the old-style and new-style solver usage. This method could be called only with the old-style usage");
        return Resolve(*p_in, initial_solution);
      }
      
      /** Method to solve a problem again, starting from the a final solution of another run.
       @param initial_solution solution to start with
       @return an output object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       @deprecated
       */
      [[deprecated("This is the old style solver interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      virtual SolverResult<Input, Output, CostStructure> Resolve(const Input& in, const Output &initial_solution) = 0;
      
      /** Virtual destructor, for inheritance. */
      virtual ~Solver() {}
      
      /** List of all solvers that have been instantiated so far. For autoloading. */
      static std::vector<Solver<Input, Output, CostStructure>*> solvers;
      
      /** Constructor.
       @param in a reference to the input object
       @param name name of the solver
       */
      [[deprecated("This is the old-style spolver interface, featuring a constant input reference, you should use the Input-less version")]]
      Solver(const Input &in, std::string name);
      
      /** Constructor.
       @param name name of the solver
       */
      Solver(std::string name);
      
      /** Get current solution (meant to be asyncrhonous) */
      virtual std::shared_ptr<Output> GetCurrentSolution() const = 0;
      
    protected:
      /** A reference to the input, for the old-style interface. */
      Input const * const p_in;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Output, class CostStructure>
    std::vector<Solver<Input, Output, CostStructure> *> Solver<Input, Output, CostStructure>::solvers;
    
    template <class Input, class Output, class CostStructure>
    Solver<Input, Output, CostStructure>::Solver(const Input &in, std::string name)
    : name(name), p_in(&in)
    {
      solvers.push_back(this);
    }
    
    template <class Input, class Output, class CostStructure>
    Solver<Input, Output, CostStructure>::Solver(std::string name)
    : name(name), p_in(nullptr)
    {
      solvers.push_back(this);
    }
  } // namespace Core
} // namespace EasyLocal
