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
  /** Returns the output by translating the best state found by the
   solver to an output object. */
  virtual const Output& GetOutput() = 0;
  const std::string name;
  void SetTimeout(double timeout);
  virtual void Solve() = 0;
  virtual ~Solver() {}
protected:
  Solver(const Input& in, std::string name);
  const Input& in; /**< A reference to the input. */
  std::chrono::milliseconds timeout, accumulated_time;
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
{
  this->timeout =  std::chrono::milliseconds::zero(); 
  this->accumulated_time =  std::chrono::milliseconds::zero();
}

template <class Input, class Output>
void Solver<Input, Output>::SetTimeout(double to)
{
  if (to > 0.0)
  {
    this->timeout = std::chrono::milliseconds((long long)(to * 1000));
  }
}


#endif // _SOLVER_HH_
