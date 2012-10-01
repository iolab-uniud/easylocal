#if !defined(_SIMPLE_LOCAL_SEARCH_HH_)
#define _SIMPLE_LOCAL_SEARCH_HH_

#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <solvers/AbstractLocalSearch.hh>
#include <runners/Runner.hh>
#include <future>

/** The Simple Local Search solver handles a simple local search algorithm
    encapsulated in a runner.
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class SimpleLocalSearch
  : public AbstractLocalSearch<Input,Output,State,CFtype>
{
public:	
  SimpleLocalSearch(const Input& in,
		    StateManager<Input,State,CFtype>& e_sm,
		    OutputManager<Input,Output,State,CFtype>& e_om,
		    std::string name);
  void Print(std::ostream& os = std::cout) const;
  void SetRunner(Runner<Input,State,CFtype>& r);
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);   
  // FIXME: all these methods should become solving parameters to sent to the Solver class
protected:
  void Go();
  void AtTimeoutExpired();
  Runner<Input,State,CFtype>* p_runner; /**< to the managed runner. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a simple local search solver by providing it links to
   a state manager, an output manager, a runner, an input,
   and an output object.

   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param r a pointer to a compatible runner
   @param in a pointer to an input object
   @param out a pointer to an output object
*/
template <class Input, class Output, class State, typename CFtype>
SimpleLocalSearch<Input,Output,State,CFtype>::SimpleLocalSearch(const Input& in,
								StateManager<Input,State,CFtype>& e_sm,
								OutputManager<Input,Output,State,CFtype>& e_om,
                                                                std::string name)
  : AbstractLocalSearch<Input,Output,State,CFtype>(in, e_sm, e_om, name, "Simple Local Search Solver"), p_runner(nullptr)
{}

template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "Simple Local Search Solver: " << this->name << " parameters" << std::endl;
  os << "Runner: " << std::endl; 
  if (this->p_runner)
    this->p_runner->ReadParameters(is, os);
  os << "Timeout: ";
  
  double to;
  is >> to;
  this->SetTimeout(to);
}



template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
  os  << "Simple Local Search Solver: " << this->GetName() << std::endl;
  if (this->p_runner)
    this->p_runner->Print(os);
  else
    os  << "<no runner attached>" << std::endl;
}


/**
   Sets the runner employed for solving the problem to the one passed as
   parameter.

   @param r the new runner to be used
*/
template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::SetRunner(Runner<Input,State,CFtype>& r)
{ this->p_runner = &r; }


template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::Go()
{
  if (!p_runner)
    // FIXME: add a more specific exception behavior
    throw std::logic_error("Runner not set in object " + this->name);
  //  if (this->timeout.IsSet())
  //{
  //  std::shared_future<CFtype> runner_result = this->p_runner->AsyncRun(std::chrono::milliseconds(static_cast<long long int>(this->timeout * 1000.0)), *this->p_current_state);
  //  this->current_state_cost = runner_result.get();
  //}
  //else
  this->current_state_cost = p_runner->Go(*this->p_current_state);
  
  *this->p_best_state = *this->p_current_state;
  this->best_state_cost = this->current_state_cost;
}

template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::AtTimeoutExpired()
{
  p_runner->Interrupt();
}

#endif // _SIMPLE_LOCAL_SEARCH_HH_
