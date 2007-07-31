#ifndef MULTIRUNNERITERATEDLOCALSEARCH_HH_
#define MULTIRUNNERITERATEDLOCALSEARCH_HH_

#include "MultiRunnerSolver.hh"

/** An Iterated Local Search solver handles both a runner encapsulating a local
    search algorithm and a kicker used for perturbing current solution.
    @ingroup Solvers */
template <class Input, class Output, class State, typename CFtype = int>
class MultiRunnerIteratedLocalSearch
  : public MultiRunnerSolver<Input,Output,State,CFtype>
{
public:
  MultiRunnerIteratedLocalSearch(const Input& i,
				 StateManager<Input,State,CFtype>& sm,
				 OutputManager<Input,Output,State,CFtype>& om,
				 std::string name = "Anonymous Multi-Runner Iterated Local Search solver");

  void Print(std::ostream& os = std::cout) const;
  void SetKicker(Kicker<Input,State,CFtype>& k);
  void SetRounds(unsigned int r);
  void Check() const throw(EasyLocalException);
  void ClearMovers();
  void RaiseTimeout();
  void ResetTimeout();
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
protected:
  void Run();
  Kicker<Input,State,CFtype>* p_kicker; /** A pointer to the managed kicker. */
  unsigned int max_idle_rounds; /**< Maximum number of runs without improvement
				   allowed. */
  Chronometer chrono; /** A chronometer */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs an iterated local search solver by providing it links to
   a state manager, an output manager, a runner, a kicker, an input,
   and an output object.

   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param r a pointer to a compatible runner
   @param k a pointer to a compatible kicker
   @param in a pointer to an input object
   @param out a pointer to an output object
*/
template <class Input, class Output, class State, typename CFtype>
MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::MultiRunnerIteratedLocalSearch(const Input& i,
										   StateManager<Input,State,CFtype>& sm,
										   OutputManager<Input,Output,State,CFtype>& om,
										   std::string name)
  : MultiRunnerSolver<Input,Output,State,CFtype>(i, sm, om, name), max_idle_rounds(1)
{
  p_kicker = NULL;
}

template <class Input, class Output, class State, typename CFtype>
void MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "Multi-runner Iterated Local Search Solver: " << this->GetName() << " parameters" << std::endl;
  os << "Runners: " << std::endl; 
	
  for (unsigned int i = 0; i < this->runners.size(); i++)
    {
      os  << "Runner[" << i << "]" << std::endl;
      this->runners[i]->ReadParameters(is, os);
    }
  os << "Kicker: " << std::endl;
  if (p_kicker)
    p_kicker->ReadParameters(is, os);
  os << "Max idle rounds: ";
  is >> max_idle_rounds;
  os << "Timeout: ";
  is >> this->timeout;
}


template <class Input, class Output, class State, typename CFtype>
void MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::RaiseTimeout()
{
  MultiRunnerSolver<Input,Output,State,CFtype>::RaiseTimeout();
  p_kicker->RaiseTimeout();
}

template <class Input, class Output, class State, typename CFtype>
void MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::SetRounds(unsigned int r)
{ max_idle_rounds = r; }


template <class Input, class Output, class State, typename CFtype>
void MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
  os  << "Multi-runner Iterated Local Search Solver: " << this->GetName() << std::endl;

  if (this->runners.size() > 0)
    for (unsigned int i = 0; i < this->runners.size(); i++)
      {
	os  << "Runner[" << i << "]" << std::endl;
	this->runners[i]->Print(os);
      }
  else
    os  << "<no runner attached>" << std::endl;
  if (p_kicker)
    p_kicker->Print(os);
  else
    os  << "<no kicker attached>" << std::endl;
  os << "Max idle rounds: " << max_idle_rounds << std::endl;
  os << "Timeout " << this->timeout << std::endl;
}

template <class Input, class Output, class State, typename CFtype>
void MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::SetKicker(Kicker<Input,State,CFtype>& k)
{ 
  p_kicker = &k; 
}

/**
   Lets the runner Go, and then collects the best state found.
*/
template <class Input, class Output, class State, typename CFtype>
void MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::Run()
{
  // TODO: try to minimize the state copying operations
  State current_state(this->in);
  CFtype current_state_cost;
    
  unsigned int idle_rounds = 0;
  unsigned int i, j;

  this->start_runner = i = 0; 
  j = this->runners.size() - 1;

  this->runners[i]->SetState(this->internal_state);
  do
    {        
      this->chrono.Reset();
      this->chrono.Start();
      this->runners[i]->Go();
      this->chrono.Stop();
#if VERBOSE >= 2
      std::cerr << "Runner: " << i << ", cost: " << this->runners[i]->GetStateCost() 
		<< ", distance " << this->sm.StateDistance( this->internal_state, this->runners[i]->GetState())
		<< " (" << this->runners[i]->GetIterationsPerformed() << " iterations, time " << chrono.TotalTime() 
		<< ")" << std::endl;
#endif
      if (this->runners[i]->GetStateCost() < this->internal_state_cost)
        {
	  this->internal_state = this->runners[i]->GetState();
	  this->internal_state_cost = this->runners[i]->GetStateCost();
        }
      j = i;
      if (this->runners[i]->LowerBoundReached())
	break;
      if (this->runners.size() > 1)
	{
	  i = (i + 1) % this->runners.size();	  
	  this->runners[i]->SetState(this->runners[j]->GetState());
	}
    }
  while (i != this->start_runner && !this->Timeout());

  current_state = this->runners[j]->GetState();
  current_state_cost = this->runners[j]->GetStateCost();    

  if (this->Timeout())
    return;

  do
    {
      if (idle_rounds % 2 == 0)
	{
#if VERBOSE >= 2
		  std::cerr << "Start kicking" << std::endl;
#endif
	  bool improved;
	  do
	    {
	      // perturb the current solution	     
	      this->chrono.Reset();
	      this->chrono.Start();
	      CFtype kick_cost =  p_kicker->SelectKick(current_state);
	      this->chrono.Stop();
	      improved = false;
	      if (LessThan(kick_cost,0))
		{
		  p_kicker->MakeKick(current_state);	   
		  current_state_cost +=  kick_cost;  
#if VERBOSE >= 2
		  std::cerr << "   Kick move, cost: " <<  current_state_cost
			    << ", distance " << this->sm.StateDistance(this->internal_state, current_state) 
			    << ", time " << this->chrono.TotalTime() << std::endl;
#endif
		  this->internal_state = current_state;
		  if (current_state_cost < this->internal_state_cost)
		    {
		      this->internal_state_cost = current_state_cost;
		      idle_rounds = 0;
		      improved = true;
		    }
		}
	      if (this->Timeout())
		return;
	    }
	  while (improved);
#if VERBOSE >= 2
		  std::cerr << "Quit kicking" << std::endl;
#endif
	}
      // and make another round
      this->start_runner = i = 0; 
      j = this->runners.size() - 1;
      this->runners[i]->SetState(this->internal_state);
      
      do
	{
	  this->chrono.Reset();
	  this->chrono.Start();
	  this->runners[i]->Go();
	  this->chrono.Stop();
#if VERBOSE >= 2
      std::cerr << "Runner: " << i << ", cost: " << this->runners[i]->GetStateCost() 
		<< ", distance " << this->sm.StateDistance( this->internal_state, this->runners[i]->GetState())
		<< " (" << this->runners[i]->GetIterationsPerformed() << " iterations, time " << chrono.TotalTime() 
		<< ")" << std::endl;
#endif
	  if (this->runners[i]->GetStateCost() <= current_state_cost)
	    {
	      current_state = this->runners[i]->GetState();
	      current_state_cost = this->runners[i]->GetStateCost();
	    }
	  j = i;
	  if (this->runners[i]->LowerBoundReached())
	    break;
	  if (this->runners.size() > 1)
	    {
	      i = (i + 1) % this->runners.size();	  
	      this->runners[i]->SetState(this->runners[j]->GetState());
	    }
	}
      while (i != this->start_runner && !this->Timeout());
        
      if (LessThan(current_state_cost,this->internal_state_cost))
        {
	  idle_rounds = 0;
	  this->internal_state = current_state;
	  this->internal_state_cost = current_state_cost;
         }
      else
	idle_rounds++;
    }
  while (idle_rounds < max_idle_rounds && !this->Timeout());
}

template <class Input, class Output, class State, typename CFtype>
void MultiRunnerIteratedLocalSearch<Input,Output,State,CFtype>::Check() const
  throw(EasyLocalException)
{
  MultiRunnerSolver<Input,Output,State,CFtype>::Check();
  if (p_kicker == NULL)
    throw EasyLocalException("RunCheck(): kicker not set in object " + this->GetName());
  //  p_kicker->Check();
}


#endif /*MULTIRUNNERITERATEDLOCALSEARCH_HH_*/
