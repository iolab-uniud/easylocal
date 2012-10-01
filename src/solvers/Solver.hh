#if !defined(_SOLVER_HH_)
#define _SOLVER_HH_

#include <EasyLocal.conf.hh>
#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <chrono>

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
  const std::string name;
  virtual Output Solve() = 0;
  virtual Output Resolve(const Output& initial_solution) = 0;
  virtual ~Solver() {}
protected:
  Solver(const Input& in, std::string name);
  const Input& in; /**< A reference to the input. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
 Constructs a solver by providing it an input.
 
 @param im a reference to the input */
template <class Input, class Output>
Solver<Input, Output>::Solver(const Input& i, std::string e_name)
: name(e_name), in(i)
{}



#endif // _SOLVER_HH_
