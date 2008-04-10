#ifndef _GENERALIZED_LOCAL_SEARCH_SOLVER_HH_
#define _GENERALIZED_LOCAL_SEARCH_SOLVER_HH_

#include "AbstractLocalSearchSolver.hh"
#include "../kickers/Kicker.hh"
#include <vector>
#include "../utils/clparser/CLParser.hh"
#include "../utils/clparser/ArgumentGroup.hh"
#include "../utils/clparser/ValArgument.hh"

enum KickStrategy {
    NO_KICKER = 0,
    DIVERSIFIER,
    INTENSIFIER,
    INTENSIFIER_RUN
};


template <class Input, class Output, class State, typename CFtype>
class GeneralizedLocalSearchObserver;

/** An Iterated Local Search solver handles both a runner encapsulating a local
    search algorithm and a kicker used for perturbing current solution.
    @ingroup Solvers */
template <class Input, class Output, class State, typename CFtype = int>
class GeneralizedLocalSearchSolver
  : public AbstractLocalSearchSolver<Input,Output,State,CFtype>
{
  friend class GeneralizedLocalSearchObserver<Input,Output,State,CFtype>;
public:
  GeneralizedLocalSearchSolver(const Input& i,
			       StateManager<Input,State,CFtype>& sm,
			       OutputManager<Input,Output,State,CFtype>& om,
			       std::string name);
  GeneralizedLocalSearchSolver(const Input& i,
			       StateManager<Input,State,CFtype>& sm,
			       OutputManager<Input,Output,State,CFtype>& om,
			       std::string name,
			       CLParser& cl);	
  void Print(std::ostream& os = std::cout) const;
  void SetKicker(Kicker<Input,State,CFtype>& k);
  void AddRunner(Runner<Input,State,CFtype>& r);
  void SetIdleRounds(unsigned int r);
  void SetRounds(unsigned int r);
  void AttachObserver(GeneralizedLocalSearchObserver<Input,Output,State,CFtype>& obs) { observer = &obs; }
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);

  void SimpleSolve(unsigned runner = 0, bool random_init = true);
  void GeneralSolve(KickStrategy kick_strategy = NO_KICKER, bool state_init = true);

protected:
  bool PerformKickRun();
  unsigned int current_runner, idle_rounds, rounds, kick_rate;

  std::vector<Runner<Input,State,CFtype>* > runners; /**< The vector of
							the linked runners. */
  Kicker<Input,State,CFtype>* p_kicker; /** A pointer to the managed kicker. */
  unsigned int max_idle_rounds, max_rounds; /**< Maximum number of runs without improvement
					       allowed. */
  GeneralizedLocalSearchObserver<Input,Output,State,CFtype>* observer;
  ArgumentGroup generalized_ls_arguments;
  ValArgument<unsigned> arg_max_idle_rounds, arg_max_rounds;
  ValArgument<double> arg_timeout;
  Chronometer chrono;
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
GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::GeneralizedLocalSearchSolver(const Input& i,
										      StateManager<Input,State,CFtype>& sm,
										      OutputManager<Input,Output,State,CFtype>& om,
										      std::string name)
  : AbstractLocalSearchSolver<Input,Output,State,CFtype>(i, sm, om, name), current_runner(0), idle_rounds(0), 
    rounds(0), kick_rate(4),
    max_idle_rounds(1), max_rounds(100), generalized_ls_arguments("gls_" + name, "gls_" + name, false), 
    arg_max_idle_rounds("max_idle_rounds", "-mir", false), arg_max_rounds("max_rounds", "-mr", false),
    arg_timeout("timeout", "to", false, 0.0)
{
  generalized_ls_arguments.AddArgument(arg_max_idle_rounds);
  generalized_ls_arguments.AddArgument(arg_max_rounds);
  generalized_ls_arguments.AddArgument(arg_timeout);
  p_kicker = NULL;
  observer = NULL;
}

template <class Input, class Output, class State, typename CFtype>
GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::GeneralizedLocalSearchSolver(const Input& i,
										      StateManager<Input,State,CFtype>& sm,
										      OutputManager<Input,Output,State,CFtype>& om,
										      std::string name,
										      CLParser& cl)
  : AbstractLocalSearchSolver<Input,Output,State,CFtype>(i, sm, om, name), current_runner(0), idle_rounds(0), rounds(0),
    max_idle_rounds(1), max_rounds(100), generalized_ls_arguments("gls_" + name, "gls_" + name, false), 
    arg_max_idle_rounds("max_idle_rounds", "-mir", false), arg_max_rounds("max_rounds", "-mr", false),
    arg_timeout("timeout", "to", false, 0.0)
{
  generalized_ls_arguments.AddArgument(arg_max_idle_rounds);
  generalized_ls_arguments.AddArgument(arg_max_rounds);
  generalized_ls_arguments.AddArgument(arg_timeout);
  cl.AddArgument(generalized_ls_arguments);
  cl.MatchArgument(generalized_ls_arguments);	
  if (generalized_ls_arguments.IsSet())
    {
      if (arg_max_idle_rounds.IsSet())
	max_idle_rounds = arg_max_idle_rounds.GetValue();
      if (arg_max_rounds.IsSet())
	max_rounds = arg_max_rounds.GetValue();
      if (arg_timeout.IsSet())
	this->SetTimeout(arg_timeout.GetValue());
    }
  p_kicker = NULL;
  observer = NULL;
}


template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  os << "Multi-runner Iterated Local Search Solver: " << this->name << " parameters" << std::endl;
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
#ifdef HAVE_PTHREAD
  os << "Timeout: ";
  is >> timeout;
  this->SetTimeout(timeout);
#endif 
}


template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::SetIdleRounds(unsigned int r)
{ max_idle_rounds = r; }

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::SetRounds(unsigned int r)
{ max_rounds = r; }


/**
   Adds the given runner to the list of the managed runners.

   @param r a pointer to a compatible runner to add
*/
template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::AddRunner(Runner<Input,State,CFtype>& r)
{
  runners.push_back(&r);
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
  os  << "Multi-runner Iterated Local Search Solver: " << this->name << std::endl;
	
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
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::SetKicker(Kicker<Input,State,CFtype>& k)
{ 
  p_kicker = &k; 
}

/**
   Solves using a single runner
*/
template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::SimpleSolve(unsigned runner, bool state_init)
{
  if (runner >= runners.size())
    throw std::logic_error("No runner set for solver " + this->name);

  chrono.Reset();
  chrono.Start();
  if (state_init)
    this->FindInitialState();
  runners[runner]->SetState(this->current_state);
  LetGo(*runners[runner]);
  this->current_state = runners[runner]->GetState();
  this->current_state_cost = runners[runner]->GetStateCost();
  this->best_state = this->current_state;
  this->best_state_cost = this->current_state_cost;
  chrono.Stop();
}

/**
   Solves using the general strategy
*/
template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::GeneralSolve(KickStrategy kick_strategy, bool state_init)

{
  bool improve_state, lower_bound_reached = false, timeout_expired = false;
  CFtype kick_cost;
  idle_rounds = 0;
  rounds = 0;

  if (kick_strategy != NO_KICKER && p_kicker == NULL)
    throw std::logic_error("No kicker set for solver " + this->name);

  chrono.Reset();
  chrono.Start();
  if (state_init)
    this->FindInitialState();

  this->best_state = this->current_state;
  this->best_state_cost = this->current_state_cost;

  do
    {
      improve_state = false;
      for (current_runner = 0; current_runner < this->runners.size(); current_runner++)
	{
	  this->runners[current_runner]->SetState(this->current_state, this->current_state_cost);
	  if (observer != NULL) observer->NotifyRunnerStart(*this);
	  timeout_expired = LetGo(*this->runners[current_runner]);
	  if (observer != NULL) observer->NotifyRunnerStop(*this);	  
	  this->current_state = this->runners[current_runner]->GetState();
	  this->current_state_cost = this->runners[current_runner]->GetStateCost();
	  if (LessThan(this->current_state_cost, this->best_state_cost))
	    {
	      improve_state = true;
	      this->best_state = this->current_state;
	      this->best_state_cost = this->current_state_cost;
	      lower_bound_reached = this->sm.LowerBoundReached(this->best_state_cost); 
	    }
	  if (lower_bound_reached || timeout_expired) break;
	}
      rounds++;
      if (improve_state)
	idle_rounds = 0;
      else
	{
	  float time = chrono.TotalTime();
	  improve_state = false;
	  idle_rounds++;
	  if (idle_rounds % kick_rate != 0) continue;
	  if (kick_strategy != NO_KICKER)
	    {
	      if (observer != NULL)
		observer->NotifyKickerStart(*this);
	      if (kick_strategy == DIVERSIFIER || kick_strategy == INTENSIFIER)
		{
		  if (kick_strategy == DIVERSIFIER)
		    kick_cost = p_kicker->RandomKick(this->current_state);
		  else // INTENSIFIER
		    kick_cost = p_kicker->SelectKick(this->current_state);
		  if (observer != NULL)
		    observer->NotifyKickStep(*this,kick_cost);
		  p_kicker->MakeKick(this->current_state);
		  this->current_state_cost += kick_cost; 
		  if (LessThan(kick_cost, static_cast<CFtype>(0))) 
		    improve_state = true;
		}
	      else if (kick_strategy == INTENSIFIER_RUN)
		{
		  improve_state = this->PerformKickRun();
		}	  
	      if (improve_state)
		{
		  this->best_state = this->current_state;
		  this->best_state_cost = this->current_state_cost;
		  lower_bound_reached = this->sm.LowerBoundReached(this->best_state_cost); 
		  idle_rounds = 0;
		}
	      if (observer != NULL)
		observer->NotifyKickerStop(*this);
	    }
#ifdef HAVE_PTHREAD
	  this->current_timeout -= (chrono.TotalTime() - time);
	  if (this->current_timeout <= 0.0)
	    {
	      timeout_expired = true;
	      this->current_timeout = 0.0;
	    }
#endif
	}
    }
  while (idle_rounds < max_idle_rounds && rounds < max_rounds && !timeout_expired && !lower_bound_reached);
  chrono.Stop();
}


template <class Input, class Output, class State, typename CFtype>
bool GeneralizedLocalSearchSolver<Input,Output,State,CFtype>::PerformKickRun()
{
  State current_state = this->current_state;
  CFtype kick_cost, current_state_cost = this->current_state_cost;
  bool improve = false;

  do
    {
      // perturb the current solution	     
      kick_cost =  p_kicker->SelectKick(current_state);
      if (LessThan(kick_cost, static_cast<CFtype>(0)))
	{
	  p_kicker->MakeKick(current_state);	   
	  current_state_cost += kick_cost; 
	  if (observer != NULL)
	    observer->NotifyKickStep(*this,kick_cost);
	  improve = true;
	}
      //       if (this->Timeout())
      // 	return improve;
    }
  while (LessThan(kick_cost,static_cast<CFtype>(0)));
  
  this->current_state = current_state;
  this->current_state_cost = current_state_cost;
  
  return improve;
}

#endif /*_GENERALIZED_LOCAL_SEARCH_SOLVER_HH_*/
