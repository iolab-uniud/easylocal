#pragma once

#include <future>

#include "helpers/statemanager.hh"
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
    template <class Input, class State, class CostStructure>
    class TokenRingSearch
    : public AbstractLocalSearch<Input, State, CostStructure>
    {
    public:
      using Runner = Runner<Input, State, CostStructure>;
      
      using AbstractLocalSearch<Input, State, CostStructure>::AbstractLocalSearch;
      
      void AddRunner(Runner &r)
      {
        p_runners.push_back(&r);

      }
      
      void RemoveRunner(const Runner &r)
      {
        
        for (auto it = p_runners.begin(); it != p_runners.end(); ++it)
        {
          if (*it == &r)
          {
            it = p_runners.erase(it);
            return;
          }
        }
        throw std::logic_error("Runner " + r->name + " was not added to the Token Ring Search");
      }
      
      void Print(std::ostream &os = std::cout) const
      {
        os << "Token Ring Solver: " << this->name << std::endl;
        unsigned int i = 0;
        for (auto p_r : p_runners)
        {
          os << "Runner [" << i++ << "]: " << std::endl;
          p_r->Print(os);
        }
        if (p_runners.size() == 0)
          os << "<no runner attached>" << std::endl;
      }
      
      void ReadParameters(std::istream &is = std::cin, std::ostream &os = std::cout)
      {
        os << "Token Ring Solver: " << this->name << " parameters" << std::endl;
        unsigned int i = 0;
        for (auto &r : p_runners)
        {
          os << "Runner [" << i++ << "]: " << std::endl;
          r->ReadParameters(is, os);
        }
      }
      
      unsigned int Round() const
      {
        return round;
      }
      
      unsigned int IdleRounds() const
      {
        return idle_rounds;
      }
      
    protected:
      void InitializeSolve()
      {
        AbstractLocalSearch<Input, State, CostStructure>::InitializeSolve();
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
      
      void Go(const Input& in)
      {
        current_runner = 0;
        do
        {
          //      std::cerr << "Rounds = " << rounds << "/" << max_rounds << ", " << idle_rounds << "/" << max_idle_rounds<< std::endl;
          this->current_state_cost = p_runners[current_runner]->Go(in, *this->p_current_state);
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
      
      void AtTimeoutExpired()
      {
        p_runners[current_runner]->Interrupt();
      }
      
      void ResetTimeout()
      {
        AbstractLocalSearch<Input, State, CostStructure>::ResetTimeout();
        for (auto &r : this->p_runners)
          r->ResetTimeout();
      }
      
      virtual std::shared_ptr<State> GetCurrentState() const
      {
        return p_runners[current_runner]->GetCurrentBestState();
      }
      
      std::vector<Runner*> p_runners; /**< pointers to the managed runner. */
      unsigned int current_runner;
      void InitializeParameters()
      {
        AbstractLocalSearch<Input, State, CostStructure>::InitializeParameters();
        max_rounds("max_rounds", "Maximum number of rounds", this->parameters);
        max_idle_rounds("max_idle_rounds", "Maximum number of idle rounds", this->parameters);
        round = 0;
        idle_rounds = 0;
      }
      
      Parameter<unsigned int> max_rounds, max_idle_rounds;
      unsigned int round;
      unsigned int idle_rounds;
    };
  } // namespace Core
} // namespace EasyLocal
