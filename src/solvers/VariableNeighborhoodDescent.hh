#if !defined(_VARIABLE_NEIGHBORHOOD_DESCENT_HH_)
#define _VARIABLE_NEIGHBORHOOD_DESCENT_HH_

#include <solvers/AbstractLocalSearch.hh>

/** The Variable Neighborhood Descent solver handles a VND algorithm
    implemented through a Kicker.
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class VariableNeighborhoodDescent
  : public AbstractLocalSearch<Input,Output,State,CFtype>
{
public:
  VariableNeighborhoodDescent(const Input& in,
	    StateManager<Input,State,CFtype>& e_sm,
	    OutputManager<Input,Output,State,CFtype>& e_om,
	    unsigned int max_k,
	    std::string name = "Anonymous Variable Neighborhood Descent solver");
	
  void Print(std::ostream& os = std::cout) const;
  void SetKicker(Kicker<Input,State,CFtype>& k);
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout); 
  void RaiseTimeout();
  void Solve();
protected:
  void Run();
  void RunCheck() const;
  Kicker<Input,State,CFtype>* p_kicker; /**< A pointer to the managed kicker. */
  unsigned int max_k;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a variable neighborhood descent solver by providing it links to
   a state manager, an output manager, a kicker, an input,
   and an output object.
 
   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param in a pointer to an input object
   @param out a pointer to an output object
*/
template <class Input, class Output, class State, typename CFtype>
VariableNeighborhoodDescent<Input,Output,State,CFtype>::VariableNeighborhoodDescent(const Input& in,
						StateManager<Input,State,CFtype>& e_sm,
						OutputManager<Input,Output,State,CFtype>& e_om,
						unsigned int max_k,
						std::string name)
  : AbstractLocalSearch<Input,Output,State,CFtype>(in, e_sm, e_om, name)
{
  p_kicker = nullptr;
  this->max_k = max_k;
}

template <class Input, class Output, class State, typename CFtype>
void VariableNeighborhoodDescent<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "Variable Neighborhood Descent Solver: " << this->name << " parameters" << std::endl;
  os << "Max k: ";
  is >> this->max_k;
  os << "Timeout: ";
  double to;
  is >> to;
  this->SetTimeout(to);}



template <class Input, class Output, class State, typename CFtype>
void VariableNeighborhoodDescent<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
  os << "Variable Neighborhood Descent: " << this->GetName() << std::endl;
  os << "Max k:" << this->max_k << std::endl;
  if (this->p_kicker)
    this->p_kicker->Print(os);
  else
    os  << "<no kicker attached>" << std::endl;
}


/**
   Sets the runner employed for solving the problem to the one passed as
   parameter.
 
   @param r the new runner to be used
*/
template <class Input, class Output, class State, typename CFtype>
void VariableNeighborhoodDescent<Input,Output,State,CFtype>::SetKicker(Kicker<Input,State,CFtype>& k)
{ this->p_kicker = &k; }


template <class Input, class Output, class State, typename CFtype>
void VariableNeighborhoodDescent<Input,Output,State,CFtype>::RaiseTimeout()
{
  AbstractLocalSearch<Input,Output,State,CFtype>::RaiseTimeout();
  this->p_kicker->RaiseTimeout();
}

template <class Input, class Output, class State, typename CFtype>
void VariableNeighborhoodDescent<Input,Output,State,CFtype>::Solve()
{
  this->FindInitialState();
  this->Run();    
}


/**
   Lets the runner Go, and then collects the best state found.
*/
template <class Input, class Output, class State, typename CFtype>
void VariableNeighborhoodDescent<Input,Output,State,CFtype>::Run()
{
  unsigned int k = 1;
  CFtype kick_cost;
  do 
    {
      this->p_kicker->SetStep(k);
      std::cerr << "Selected Kick: " << k << " ";
      kick_cost = this->p_kicker->FirstImprovingKick(this->current_state);
      std::cerr << kick_cost << std::endl;
      if (LessThan(kick_cost,0))
	{
	  this->p_kicker->MakeKick(this->current_state);
	  this->current_state_cost += kick_cost;
	  std::cerr << "Performed Kick: [" << this->current_state_cost << "]: " << k << std::endl;
	  this->p_kicker->PrintKick(std::cerr);
	  k = 1;
	}
      else
	k++;
    }
  while (k <= this->max_k && !this->sm.LowerBoundReached(this->current_state_cost));
}

template <class Input, class Output, class State, typename CFtype>
void VariableNeighborhoodDescent<Input,Output,State,CFtype>::RunCheck() const

{
  AbstractLocalSearch<Input,Output,State,CFtype>::RunCheck();
  if (this->p_kicker == nullptr)
    throw std::logic_error("RunCheck(): kicker not set in object " + this->GetName());
}

#endif // _VARIABLE_NEIGHBORHOOD_DESCENT_HH_
