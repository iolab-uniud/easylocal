#ifndef VNDSOLVER_HH_
#define VNDSOLVER_HH_

#include "AbstractLocalSearchSolver.hh"
//#include "../runners/Runner.hh"
#include <stdexcept>

/** The Variable Neighborhood Descent solver handles a VND algorithm
    implemented through a Kicker.
    @ingroup Solvers
*/
template <class Input, class Output, class State, typename CFtype = int>
class VNDSolver
  : public AbstractLocalSearchSolver<Input,Output,State,CFtype>
{
public:
  VNDSolver(const Input& in,
	    StateManager<Input,State,CFtype>& e_sm,
	    OutputManager<Input,Output,State,CFtype>& e_om,
	    unsigned max_k,
	    std::string name = "Anonymous Variable Neighborhood Descent solver");
	
  void Print(std::ostream& os = std::cout) const;
  void SetKicker(Kicker<Input,State,CFtype>& k);
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout); 
  void RaiseTimeout();
  void Solve();

protected:
  void RunCheck() const;
  Kicker<Input,State,CFtype>* p_kicker; /**< A pointer to the managed kicker. */
  unsigned max_k;
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
VNDSolver<Input,Output,State,CFtype>::VNDSolver(const Input& in,
						StateManager<Input,State,CFtype>& e_sm,
						OutputManager<Input,Output,State,CFtype>& e_om,
						unsigned max_k,
						std::string name)
  : AbstractLocalSearchSolver<Input,Output,State,CFtype>(in, e_sm, e_om, name)
{
  p_kicker = NULL;
  this->max_k = max_k;
}

template <class Input, class Output, class State, typename CFtype>
void VNDSolver<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "Variable Neighborhood Descent Solver: " << this->name << " parameters" << std::endl;
  os << "Max k: ";
  is >> this->max_k;
#ifdef HAVE_PTHREAD
  os << "Timeout: ";
  is >> this->timeout;
  this->current_timeout = this->timeout;
#endif 
}



template <class Input, class Output, class State, typename CFtype>
void VNDSolver<Input,Output,State,CFtype>::Print(std::ostream& os) const
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
void VNDSolver<Input,Output,State,CFtype>::SetKicker(Kicker<Input,State,CFtype>& k)
{ this->p_kicker = &k; }


template <class Input, class Output, class State, typename CFtype>
void VNDSolver<Input,Output,State,CFtype>::RaiseTimeout()
{
  AbstractLocalSearchSolver<Input,Output,State,CFtype>::RaiseTimeout();
  this->p_kicker->RaiseTimeout();
}

/**
   Lets the runner Go, and then collects the best state found.
*/
template <class Input, class Output, class State, typename CFtype>
void VNDSolver<Input,Output,State,CFtype>::Solve()
{
  throw std::runtime_error("This method have to be reimplemented");
  /*  unsigned k = 1;
  CFtype kick_cost;
  do 
    {
      this->p_kicker->SetStep(k);
      kick_cost = this->p_kicker->BestKick(this->internal_state);
#if VERBOSE >= 2
      std::cerr << "Selected Kick: " << k << " " << kick_cost << std::endl;
      this->p_kicker->PrintKick(std::cerr);
#endif
      if (kick_cost < 0.0)
	{
	  this->p_kicker->MakeKick(this->internal_state);
	  this->internal_state_cost += kick_cost;
#if VERBOSE >= 2
	  std::cerr << "Performed Kick: [" << this->internal_state_cost << "]: " << k;
	  this->p_kicker->PrintKick(std::cerr);
#endif
	  k = 1;
	}
      else
	k++;
    }
    while (k <= this->max_k && !this->sm.LowerBoundReached(this->internal_state_cost)); */
}

template <class Input, class Output, class State, typename CFtype>
void VNDSolver<Input,Output,State,CFtype>::RunCheck() const

{
  AbstractLocalSearchSolver<Input,Output,State,CFtype>::RunCheck();
  if (this->p_kicker == NULL)
    throw std::logic_error("RunCheck(): kicker not set in object " + this->GetName());
}

#endif /*VNDSOLVER_HH_*/
