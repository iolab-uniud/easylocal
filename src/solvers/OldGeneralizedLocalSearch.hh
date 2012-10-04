#if !defined(_GENERALIZED_LOCAL_SEARCH_HH_)
#define _GENERALIZED_LOCAL_SEARCH_HH_

#include <solvers/AbstractLocalSearch.hh>
#include <kickers/Kicker.hh>
#include <observers/GeneralizedLocalSearchObserver.hh>
#include <vector>


enum KickStrategy
{
  NO_KICKER = 0,
  DIVERSIFIER,
  DIVERSIFIER_AT_EVERY_ROUND,
  INTENSIFIER,
  INTENSIFIER_RUN
};

/** An Iterated Local Search solver handles both a runner encapsulating a local
 search algorithm and a kicker used for perturbing current solution.
 @ingroup Solvers */
template <class Input, class Output, class State, typename CFtype>
class GeneralizedLocalSearch
: public AbstractLocalSearch<Input,Output,State,CFtype>
{
  friend class GeneralizedLocalSearchObserver<Input,Output,State,CFtype>;
public:

  /** Constructor.
   @param i a pointer to an input object
   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param name the name of this solver
   @param cl a pointer to a CLParser
   */
  GeneralizedLocalSearch(const Input& i, 
                         StateManager<Input,State,CFtype>& sm,
                         OutputManager<Input,Output,State,CFtype>& om,
                         std::string name,
                         CLParser& cl = CLParser::empty);
  
  /** Prints the state of this solver onto an output stream. 
   @param os the output stream to print on
   */
  void Print(std::ostream& os = std::cout) const;
  
  /** Sets the kicker to use in this solver.
   @param k the kicker to use
   @param kick_rate the kick rate to use (default 2)
   */
  void SetKicker(Kicker<Input,State,CFtype>& k, unsigned int kick_rate = 2);
  
  /** Adds a search strategy (runner) to this solver.
   @param r the runner to add
   */
  void AddRunner(Runner<Input,State,CFtype>& r);
  
  /** Removes all runners. */
  void ClearRunners();
  
  /** Sets the maximum number of idle rounds. 
   @param r number of idle rounds
   */
  void SetIdleRounds(unsigned int r) { max_idle_rounds = r; }
  
  /** Sets the maximum number of rounds. 
   @param r number of idle rounds
   */
  void SetRounds(unsigned int r) { max_rounds = r; }
  
  /** Gets the number of rounds. */
  unsigned int GetRounds() const { return rounds; }
  
  /** Gets the number of idle rounds. */
  unsigned int GetIdleRounds() const { return idle_rounds; }
  
  /** Get the number of restarts. */
  unsigned int GetRestarts() const { return restarts; }
  
  /** Attaches a compatible observer to this solver.
   @param obs observer to attach 
   */
  void AttachObserver(GeneralizedLocalSearchObserver<Input,Output,State,CFtype>& obs) { observer = &obs; }
  
  /** Method to read the parameters from an input stream. 
   @param is stream from which to load the parameters
   @param os stream to print the parameter requests
   */
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  
  /** Gets the number of kick rounds. */
  unsigned int GetKickRounds() const { return kick_rounds; }
  
  /** Gets the number of improving kick rounds. */
  unsigned int GetKickImprovingRounds() const { return kick_improving_rounds; }
  
  // FIXME: all these methods should become solving parameters to sent to the Solver class
  void Solve() { this->SimpleSolve(); }
  
  // FIXME: all these methods should become solving parameters to sent to the Solver class
  void SimpleSolve(unsigned int runner = 0, unsigned int init_state = 1); // 0: leave unchanged, 1: random, 2 :greedy
  void MultiStartSimpleSolve(unsigned int runner = 0, unsigned int trials = 1);
  void MultiStartGeneralSolve(KickStrategy kick_strategy = NO_KICKER, unsigned int trials = 1);
  void GeneralSolve(KickStrategy kick_strategy = NO_KICKER, bool state_init = true);
  void IteratedSolve(unsigned int runner = 0, bool state_init = true);
  
protected:
  
  bool PerformKickRun();
  unsigned int current_runner, idle_rounds, restarts, rounds, kick_rate, kick_rounds, kick_improving_rounds;
  
  std::vector<Runner<Input,State,CFtype>* > runners; /**< The vector of
                                                      the linked runners. */
  Kicker<Input,State,CFtype>* p_kicker; /** A pointer to the managed kicker. */
  unsigned int max_idle_rounds, max_rounds; /**< Maximum number of runs without improvement
                                             allowed. */
  GeneralizedLocalSearchObserver<Input,Output,State,CFtype>* observer;
  ArgumentGroup generalized_ls_arguments;
  ValArgument<unsigned int> arg_max_idle_rounds, arg_max_rounds;
  ValArgument<float> arg_timeout;

  /** Chronometer. */
  std::chrono::high_resolution_clock::time_point begin;
  std::chrono::high_resolution_clock::time_point end;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class Output, class State, typename CFtype>
GeneralizedLocalSearch<Input,Output,State,CFtype>::GeneralizedLocalSearch(const Input& i,
                                                                          StateManager<Input,State,CFtype>& sm,
                                                                          OutputManager<Input,Output,State,CFtype>& om,
                                                                          std::string name,
                                                                          CLParser& cl)
: AbstractLocalSearch<Input,Output,State,CFtype>(i, sm, om, name), current_runner(0), idle_rounds(0), rounds(0),
max_idle_rounds(1), max_rounds(100), generalized_ls_arguments("gls_" + name, "gls_" + name, false), 
arg_max_idle_rounds("max_idle_rounds", "mir", false), arg_max_rounds("max_rounds", "mr", false),
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
  p_kicker = nullptr;
  observer = nullptr;
}


template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
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
  double timeout;
  os << "Timeout: ";
  is >> timeout;
  this->SetTimeout(timeout);
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::AddRunner(Runner<Input,State,CFtype>& r)
{
  runners.push_back(&r);
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::ClearRunners()
{
  runners.clear();
  current_runner = 0;
}

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
  os  << "Generalized Local Search Solver: " << this->name << std::endl;
	
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
void GeneralizedLocalSearch<Input,Output,State,CFtype>::SetKicker(Kicker<Input,State,CFtype>& k, unsigned int kr)
{ 
  p_kicker = &k; 
  kick_rate = kr;
}

/**
 Solves using a single runner
 */
template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::SimpleSolve(unsigned int runner, unsigned int init_state)
{
  if (runner >= runners.size())
    throw std::logic_error("No runner set for solver " + this->name);
  
  begin = std::chrono::high_resolution_clock::now();
  if (init_state == 1)
    this->FindInitialState(true);
  else if (init_state == 2)
    this->FindInitialState(false);
  
  // else: do not call FindInitialState, leave initial state unchanged
  
  runners[runner]->SetState(this->current_state);
  if (observer != nullptr) observer->NotifyRunnerStart(*this);
  runners[runner]->Go();
  if (observer != nullptr) observer->NotifyRunnerStop(*this);	  
  this->current_state = runners[runner]->GetState();
  this->current_state_cost = runners[runner]->GetStateCost();
  this->best_state = this->current_state;
  this->best_state_cost = this->current_state_cost;
  end = std::chrono::high_resolution_clock::now();
}

/**
 Solves using a single runner, but making many starts
 */
template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::MultiStartSimpleSolve(unsigned runner, unsigned trials)
{
  bool timeout_expired = false;
  unsigned int t;
  if (runner >= runners.size())
    throw std::logic_error("No runner set for solver " + this->name);
  
  begin = std::chrono::high_resolution_clock::now();
  for (t = 0; t < trials; t++)
  {
    if (observer != nullptr) observer->NotifyRestart(*this, t);
    this->FindInitialState();
    runners[runner]->SetState(this->current_state);
    if (observer != nullptr) observer->NotifyRunnerStart(*this);
    
    // Run doesn't exists, let's use Runner::Go instead
    // timeout_expired = Run(*runners[runner]);
    timeout_expired = runners[runner]->Go();
    
    if (observer != nullptr) observer->NotifyRunnerStop(*this);	  
    this->current_state = runners[runner]->GetState();
    this->current_state_cost = runners[runner]->GetStateCost();
    
    if (t == 0 || LessThan(this->current_state_cost, this->best_state_cost))
    {
      this->best_state = this->current_state;
      this->best_state_cost = this->current_state_cost;
      if (this->sm.LowerBoundReached(this->best_state_cost))
        break;
    }
    if (timeout_expired)
      break;
    restarts++;
  }
  end = std::chrono::high_resolution_clock::now();
}

/**
 Solves making many starts
 */

template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::MultiStartGeneralSolve(KickStrategy kick_strategy, unsigned int trials)
{
  State global_best_state(this->in);
  CFtype global_best_state_cost;
  bool timeout_expired = false; 
  
  begin = std::chrono::high_resolution_clock::now();
  for (unsigned int t = 0; t < trials; t++)
  {
    if (observer != nullptr) observer->NotifyRestart(*this, t);
    this->GeneralSolve(kick_strategy);
    if (t == 0 || LessThan(this->best_state_cost, global_best_state_cost))
    {
      global_best_state = this->best_state;
      global_best_state_cost = this->best_state_cost;
      if (this->sm.LowerBoundReached(global_best_state_cost))
        break;
    }
    if (this->timeout_set) 
    {
      if (this->current_timeout <= 0.0)
	    {
	      timeout_expired = true;
	      this->current_timeout = 0.0;
	    }
    }
    if (timeout_expired)
      break;
    restarts++;
  }
  this->best_state = global_best_state;
  this->best_state_cost	= global_best_state_cost;
  end = std::chrono::high_resolution_clock::now();
}

/**
 Solves using the general strategy
 */
template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::GeneralSolve(KickStrategy kick_strategy, bool state_init)

{
  bool improve_state, lower_bound_reached = false, timeout_expired = false;
  CFtype kick_cost;
  
  // Debug
  
  kick_rounds = 0, kick_improving_rounds = 0;
  
  idle_rounds = 0;
  rounds = 0;
  
  if (kick_strategy != NO_KICKER && p_kicker == nullptr)
    throw std::logic_error("No kicker set for solver " + this->name);

  begin = std::chrono::high_resolution_clock::now();
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
      if (observer != nullptr) observer->NotifyRunnerStart(*this);
      runners[current_runner]->Go(rounds, max_rounds);
      if (observer != nullptr) observer->NotifyRunnerStop(*this);	  
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
    current_runner = 0;
    rounds++;
    if (observer != nullptr) observer->NotifyRound(*this);	        
    if (improve_state)
      idle_rounds = 0;
    else
      idle_rounds++;
    
    if (!improve_state || kick_strategy == DIVERSIFIER_AT_EVERY_ROUND)
    {
      improve_state = false;
      if (idle_rounds % kick_rate != 0) continue;
      if (kick_strategy != NO_KICKER)
      {
        kick_rounds++;
        if (observer != nullptr)
          observer->NotifyKickerStart(*this);
        if (kick_strategy == DIVERSIFIER || kick_strategy == DIVERSIFIER_AT_EVERY_ROUND || kick_strategy == INTENSIFIER)
        {
          if (kick_strategy == DIVERSIFIER || kick_strategy == DIVERSIFIER_AT_EVERY_ROUND)
            kick_cost = p_kicker->RandomKick(this->current_state);
          else // INTENSIFIER
            kick_cost = p_kicker->SelectKick(this->current_state);
          if (observer != nullptr)
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
          kick_improving_rounds++;
        }
        if (observer != nullptr)
          observer->NotifyKickerStop(*this);
      }
    }
  }
  while (idle_rounds < max_idle_rounds && rounds < max_rounds && !timeout_expired && !lower_bound_reached);
  end = std::chrono::high_resolution_clock::now();
}

/**
 Solves using a single runner and iterated strategy
 */
template <class Input, class Output, class State, typename CFtype>
void GeneralizedLocalSearch<Input,Output,State,CFtype>::IteratedSolve(unsigned int runner, bool state_init)

{
  bool improve_state, lower_bound_reached = false, timeout_expired = false;
  
  if (runner >= runners.size())
    throw std::logic_error("No runner set for solver " + this->name);
  
  idle_rounds = 0;
  rounds = 0;
  
  begin = std::chrono::high_resolution_clock::now();
  if (state_init)
    this->FindInitialState();
  
  this->best_state = this->current_state;
  this->best_state_cost = this->current_state_cost;
  
  do
  {
    improve_state = false;
    this->runners[runner]->SetState(this->current_state, this->current_state_cost);
    if (observer != nullptr) observer->NotifyRunnerStart(*this);
    timeout_expired = LetGo(*this->runners[runner], rounds, max_rounds);
    if (observer != nullptr) observer->NotifyRunnerStop(*this);	  
    this->current_state = this->runners[runner]->GetState();
    this->current_state_cost = this->runners[runner]->GetStateCost();
    if (LessThan(this->current_state_cost, this->best_state_cost))
    {
      improve_state = true;
      this->best_state = this->current_state;
      this->best_state_cost = this->current_state_cost;
      lower_bound_reached = this->sm.LowerBoundReached(this->best_state_cost); 
    }
    if (lower_bound_reached || timeout_expired) break;
    rounds++;
    if (observer != nullptr) observer->NotifyRound(*this);	        
    if (improve_state)
      idle_rounds = 0;
    else
      idle_rounds++;
  }
  while (idle_rounds < max_idle_rounds && rounds < max_rounds && !timeout_expired && !lower_bound_reached);
  end = std::chrono::high_resolution_clock::now();
}

template <class Input, class Output, class State, typename CFtype>
bool GeneralizedLocalSearch<Input,Output,State,CFtype>::PerformKickRun()
{
  State current_state = this->current_state;
  CFtype kick_cost, current_state_cost = this->current_state_cost;
  bool improve = false;
  
  begin = std::chrono::high_resolution_clock::now();
  do
  {
    // perturb the current solution	     
    kick_cost =  p_kicker->SelectKick(current_state);
    if (LessThan(kick_cost, static_cast<CFtype>(0)))
    {
      p_kicker->MakeKick(current_state);	   
      current_state_cost += kick_cost; 
      if (observer != nullptr)
        observer->NotifyKickStep(*this,kick_cost);
      improve = true;
    }
  }
  while (LessThan(kick_cost,static_cast<CFtype>(0)));
  end = std::chrono::high_resolution_clock::now();

  this->current_state = current_state;
  this->current_state_cost = current_state_cost;
  
  return improve;
}

#endif // _GENERALIZED_LOCAL_SEARCH_HH_
