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
template <class Input, class Output, class CostStructure = DefaultCostStructure<int>>
class Solver
{
public:
  /** Name of the solver. */
  const std::string name;

  /** Method to solve the problem which has been set up in the constructor.
       @return an output object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
  virtual SolverResult<Input, Output, CostStructure> Solve() = 0;

  /** Method to solve a problem again, starting from the a final solution of another run.
       @param initial_solution solution to start with
       @return an output object containing the result
       @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
       @throw IncorrectParameterValue if one of the parameters has an incorrect value
       */
  virtual SolverResult<Input, Output, CostStructure> Resolve(const Output &initial_solution) = 0;

  /** Virtual destructor, for inheritance. */
  virtual ~Solver() {}

  /** List of all solvers that have been instantiated so far. For autoloading. */
  static std::vector<Solver<Input, Output, CostStructure> *> solvers;

  /** Constructor.
       @param in a reference to the input object
       @param name name of the constructor
       */
  Solver(const Input &in, std::string name);

  /** Get current solution (meant to be asyncrhonous) */
  virtual std::shared_ptr<Output> GetCurrentSolution() const = 0;

protected:
  /** A reference to the input. */
  const Input &in;
};

/*************************************************************************
     * Implementation
     *************************************************************************/

template <class Input, class Output, class CostStructure>
std::vector<Solver<Input, Output, CostStructure> *> Solver<Input, Output, CostStructure>::solvers;

template <class Input, class Output, class CostStructure>
Solver<Input, Output, CostStructure>::Solver(const Input &i, std::string e_name)
    : name(e_name), in(i)
{
  solvers.push_back(this);
}
} // namespace Core
} // namespace EasyLocal
