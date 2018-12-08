#pragma once

#include <future>

#include "helpers/statemanager.hh"
#include "helpers/outputmanager.hh"
#include "solvers/abstractlocalsearch.hh"
#include "runners/runner.hh"

namespace EasyLocal
{

namespace Core
{

/** The Simple Local Search solver handles a simple local search algorithm
     encapsulated in a runner.
     @ingroup Solvers
     */
template <class Input, class Output, class State, class CostStructure = DefaultCostStructure<int>>
class TokenRingSearch
    : public AbstractLocalSearch<Input, Output, State, CostStructure>
{
public:
  typedef Runner<Input, State, CostStructure> RunnerType;
  TokenRingSearch(const Input &in,
                  StateManager<Input, State, CostStructure> &e_sm,
                  OutputManager<Input, Output, State> &e_om,
                  std::string name);
  void AddRunner(RunnerType &r);
  void RemoveRunner(const RunnerType &r);
  void Print(std::ostream &os = std::cout) const;
  void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout);
  unsigned int Round() const { return round; }
  unsigned int IdleRounds() const { return idle_rounds; }

protected:
  void InitializeSolve();
  void Go();
  void AtTimeoutExpired();
  void ResetTimeout();
  virtual std::shared_ptr<State> GetCurrentState() const;

  std::vector<RunnerType *> p_runners; /**< pointers to the managed runner. */
  unsigned int current_runner;
  void InitializeParameters();
  Parameter<unsigned int> max_rounds, max_idle_rounds;
  unsigned int round;
  unsigned int idle_rounds;
};

/*************************************************************************
     * Implementation
     *************************************************************************/

/**
     Constructs a token ring solver by providing it links to
     a state manager, an output manager, a runner, an input,
     and an output object.
     
     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @param r a pointer to a compatible runner
     @param in a pointer to an input object
     @param out a pointer to an output object
     */
template <class Input, class Output, class State, class CostStructure>
TokenRingSearch<Input, Output, State, CostStructure>::TokenRingSearch(const Input &in,
                                                                      StateManager<Input, State, CostStructure> &e_sm,
                                                                      OutputManager<Input, Output, State> &e_om,
                                                                      std::string name)
    : AbstractLocalSearch<Input, Output, State, CostStructure>(in, e_sm, e_om, name)
{
}

template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::InitializeParameters()
{
  AbstractLocalSearch<Input, Output, State, CostStructure>::InitializeParameters();
  max_rounds("max_rounds", "Maximum number of rounds", this->parameters);
  max_idle_rounds("max_idle_rounds", "Maximum number of idle rounds", this->parameters);
  round = 0;
  idle_rounds = 0;
}

template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::ReadParameters(std::istream &is, std::ostream &os)
{
  os << "Token Ring Solver: " << this->name << " parameters" << std::endl;
  unsigned int i = 0;
  for (auto &r : p_runners)
  {
    os << "Runner [" << i++ << "]: " << std::endl;
    r->ReadParameters(is, os);
  }
}

template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::Print(std::ostream &os) const
{
  os << "Token Ring Solver: " << this->name << std::endl;
  unsigned int i = 0;
  for (const RunnerType *p_r : p_runners)
  {
    os << "Runner [" << i++ << "]: " << std::endl;
    p_r->Print(os);
  }
  if (p_runners.size() == 0)
    os << "<no runner attached>" << std::endl;
}

/**
     Adds to the runner list employed for solving the problem to the one passed as
     parameter.
     
     @param r the new runner to be used
     */
template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::AddRunner(RunnerType &r)
{
  p_runners.push_back(&r);
}

/**
     Removes from the runner list employed for solving the problem the one passed as
     parameter.
     
     @param r the runner to remove
     */
template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::RemoveRunner(const RunnerType &r)
{
  typename std::vector<RunnerType *>::const_iterator it;
  for (it = p_runners.begin(); it != p_runners.end(); ++it)
  {
    if (*it == &r)
      break;
  }
  if (it == p_runners.end())
    throw std::logic_error("Runner " + r->name + " was not added to the Token Ring Search");
  p_runners.erase(it);
}

template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::InitializeSolve()
{
  AbstractLocalSearch<Input, Output, State, CostStructure>::InitializeSolve();
  if (max_idle_rounds.IsSet() && max_idle_rounds == 0)
    throw IncorrectParameterValue(max_idle_rounds, "It should be greater than zero");
  if (max_rounds.IsSet() && max_rounds == 0)
    throw IncorrectParameterValue(max_rounds, "It should be greater than zero");
  if (p_runners.size() == 0)
    // FIXME: add a more specific exception behavior
    throw std::logic_error("No runner set in object " + this->name);
  round = 0;
  idle_rounds = 0;
}

template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::Go()
{
  current_runner = 0;
  do
  {
    //      std::cerr << "Rounds = " << rounds << "/" << max_rounds << ", " << idle_rounds << "/" << max_idle_rounds<< std::endl;
    this->current_state_cost = p_runners[current_runner]->Go(*this->p_current_state);
    round++;
    idle_rounds++;
    if (this->current_state_cost <= this->best_state_cost) // the less or equal is here for diversification purpose
    {
      if (this->current_state_cost < this->best_state_cost)
        idle_rounds = 0;
      *this->p_best_state = *this->p_current_state;
      this->best_state_cost = this->current_state_cost;
    }
    current_runner = (current_runner + 1) % p_runners.size();
  } while (idle_rounds < max_idle_rounds && round < max_rounds);
}

template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::AtTimeoutExpired()
{
  p_runners[current_runner]->Interrupt();
}

template <class Input, class Output, class State, class CostStructure>
void TokenRingSearch<Input, Output, State, CostStructure>::ResetTimeout()
{
  Interruptible<int>::ResetTimeout();
  for (auto &r : this->p_runners)
    r->ResetTimeout();
}

template <class Input, class Output, class State, class CostStructure>
std::shared_ptr<State> TokenRingSearch<Input, Output, State, CostStructure>::GetCurrentState() const
{
  return p_runners[current_runner]->GetCurrentBestState();
}
} // namespace Core
} // namespace EasyLocal
