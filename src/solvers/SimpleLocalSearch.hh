#if !defined(_SIMPLE_LOCAL_SEARCH_HH_)
#define _SIMPLE_LOCAL_SEARCH_HH_

#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <solvers/AbstractLocalSearch.hh>
#include <runners/Runner.hh>

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
		    std::string name,
		    CLParser& cl = CLParser::empty);
  void Print(std::ostream& os = std::cout) const;
  void SetRunner(Runner<Input,State,CFtype>& r);
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);   
  // FIXME: all these methods should become solving parameters to sent to the Solver class
  void Solve();
protected:
  void Run();
  void Check() const;
  Runner<Input,State,CFtype>* p_runner; /**< A pointer to the managed runner. */
  ArgumentGroup simple_ls_arguments;
  ValArgument<float> arg_timeout;	
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
								std::string name,
                CLParser& cl)
  : AbstractLocalSearch<Input,Output,State,CFtype>(in, e_sm, e_om, name),
    simple_ls_arguments("sls_" + name, "sls_" + name, false), arg_timeout("timeout", "to", false, 0.0)
{
  simple_ls_arguments.AddArgument(arg_timeout);
  cl.AddArgument(simple_ls_arguments);
  cl.MatchArgument(simple_ls_arguments);	
  if (simple_ls_arguments.IsSet())
    if (arg_timeout.IsSet())
      this->SetTimeout(arg_timeout.GetValue());
  p_runner = nullptr;
}

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
void SimpleLocalSearch<Input,Output,State,CFtype>::Solve() {
  this->FindInitialState();
  p_runner->SetState(this->current_state);
  p_runner->Go();
  this->current_state = this->p_runner->GetState();
  this->current_state_cost = this->p_runner->GetStateCost();
  this->best_state = this->current_state;
  this->best_state_cost = this->current_state_cost;
}



/**
   Lets the runner Go, and then collects the best state found.
*/
template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::Run()
{
  this->p_runner->SetState(this->internal_state);
  // LetGo(*this->p_runner);
  // TODO: LetGo is no longer defined, in the meanwhile ...
  this->p_runner->Go();
  
  this->internal_state = this->p_runner->GetState();
  this->internal_state_cost = this->p_runner->GetStateCost();
}

template <class Input, class Output, class State, typename CFtype>
void SimpleLocalSearch<Input,Output,State,CFtype>::Check() const

{
  AbstractLocalSearch<Input,Output,State,CFtype>::Check();
  if (this->p_runner == nullptr)
    throw std::logic_error("Check(): runner not set in object " + this->name);
  this->p_runner->Check();
}

#endif // _SIMPLE_LOCAL_SEARCH_HH_
