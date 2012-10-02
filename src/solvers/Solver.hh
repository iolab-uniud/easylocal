#if !defined(_SOLVER_HH_)
#define _SOLVER_HH_

#include <EasyLocal.conf.hh>
#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <utils/Parameter.hh>

/** A Solver represents the external layer of EasyLocal++; it 
 implements the Abstract Solver interface and furthermore is
 parametrized with the Input and Output of the problem.  
 @ingroup
 Solvers 
 */
template <class Input, class Output>
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
  virtual Output Solve() throw (ParameterNotSet, IncorrectParameterValue) = 0;
  
  /** Method to solve a problem again, starting from the a final solution of another run.
   @param initial_solution solution to start with
   @return an output object containing the result
   @throw ParameterNotSet if one of the parameters needed by the solver or the runner (or other components) hasn't been set.
   @throw IncorrectParameterValue if one of the parameters has an incorrect value
   */
  virtual Output Resolve(const Output& initial_solution) throw (ParameterNotSet, IncorrectParameterValue) = 0;
  
  /** Virtual destructor, for inheritance. */
  virtual ~Solver() {}
  
  /** List of all solvers that have been instantiated so far. For autoloading. */
  static std::vector<Solver<Input,Output>*> solvers;
  
protected:
  
  /** Constructor.
   @param in a reference to the input object
   @param name name of the constructor
   */
  Solver(const Input& in, std::string name);
  
  /** A reference to the input. */
  const Input& in;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class Output>
std::vector<Solver<Input,Output>*> Solver<Input, Output>::solvers;

template <class Input, class Output>
Solver<Input, Output>::Solver(const Input& i, std::string e_name)
: name(e_name), in(i)
{
  solvers.push_back(this);
}

#endif // _SOLVER_HH_
