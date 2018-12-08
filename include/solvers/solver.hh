#pragma once

#include <tuple>

#include "helpers/statemanager.hh"
#include "helpers/outputmanager.hh"
#include "utils/parameter.hh"
#include "utils/deprecationhandler.hh"

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
    class Solver : protected DeprecationHandler<_Input>
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
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      SolverResult<Input, Output, CostStructure> Solve()
      {
        return Solve(this->GetInput());
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
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      SolverResult<Input, Output, CostStructure> Resolve(const Output &initial_solution)
      {
        return Resolve(this->GetInput(), initial_solution);
      }
      
      /** Method to solve a problem again, starting from the a final solution of another run.
       @param initial_solution solution to start with
       @return an output object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      virtual SolverResult<Input, Output, CostStructure> Resolve(const Input& in, const Output &initial_solution) = 0;
      
      /** Virtual destructor, for inheritance. */
      virtual ~Solver() {}
      
      /** List of all solvers that have been instantiated so far. For autoloading. */
      static std::vector<Solver<Input, Output, CostStructure>*> solvers;
      
      /** Constructor.
       @param in a reference to the input object
       @param name name of the solver
       */
      [[deprecated("This is the old style easylocal interface, it might still be used, however we advise to upgrade to Input-less class and Input-aware methods")]]
      Solver(const Input &in, std::string name);
      
      /** Constructor.
       @param name name of the solver
       */
      Solver(std::string name);
      
      /** Get current solution (meant to be asyncrhonous) */
      virtual std::shared_ptr<Output> GetCurrentSolution() const = 0;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Output, class CostStructure>
    std::vector<Solver<Input, Output, CostStructure> *> Solver<Input, Output, CostStructure>::solvers;
    
    template <class Input, class Output, class CostStructure>
    Solver<Input, Output, CostStructure>::Solver(const Input &in, std::string name) : DeprecationHandler<Input>(in), name(name)
    {
      solvers.push_back(this);
    }
    
    template <class Input, class Output, class CostStructure>
    Solver<Input, Output, CostStructure>::Solver(std::string name)
    : name(name)
    {
      solvers.push_back(this);
    }
  } // namespace Core
} // namespace EasyLocal
