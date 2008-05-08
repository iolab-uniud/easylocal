#ifndef SOLVER_HH_
#define SOLVER_HH_

#include <EasyLocal.conf.hh>
#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>

/** A Solver represents the external layer of EasyLocal++; it
    implements the Abstract Solver interface and furthermore is
    parametrized with the Input and Output of the problem.  @ingroup
    Solvers 
*/
template <class Input, class Output>
class Solver
{
public:
  //  virtual void Solve() = 0;
    /** Returns the output by translating the best state found by the
        solver to an output object. */
    virtual const Output& GetOutput() = 0;
  const std::string name;
  void SetTimeout(double timeout);
protected:
  Solver(const Input& in, std::string name);
  virtual ~Solver() {}
  const Input& in; /**< A reference to the input manager. */
#if defined(HAVE_PTHREAD)
protected:
  /**< This variable will be shared among runners (and possibly other lower-level components) and controls their termination. */
  RWLockVariable<bool> termination_request;
  /**< This variable avoids active waiting of runners termination. */
  ConditionVariable runner_termination;  
  double timeout, current_timeout;
  bool timeout_set;
#endif
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
    Constructs a solver by providing it an input manager.

    @param im a reference to the input manager
*/
template <class Input, class Output>
Solver<Input, Output>::Solver(const Input& i, std::string e_name)
: name(e_name), in(i)
{
#ifdef HAVE_PTHREAD
  this->timeout = 0.0; 
  this->current_timeout = 0.0;
	this->timeout_set = false;
#endif
}

template <class Input, class Output>
void Solver<Input, Output>::SetTimeout(double to)
{
#ifdef HAVE_PTHREAD
	if (to > 0.0)
	{
		this->timeout = to;
		this->current_timeout = to;
		this->timeout_set = true;
	}
	else
		this->timeout_set = false;
#endif
}


#endif /*SOLVER_HH_*/
