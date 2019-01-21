#pragma once

#include <stdexcept>
#include <list>
#include <queue>

#include "runners/moverunner.hh"
#include "helpers/statemanager.hh"
#include "helpers/neighborhoodexplorer.hh"
#include "helpers/multimodalneighborhoodexplorer.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    template <class Move>
    struct TabuListItem
    {
      TabuListItem() : tenure(0) {}
      TabuListItem(const Move &move, unsigned long int tenure) : move(move), tenure(tenure) {}
      Move move;
      unsigned long int tenure;
      
      struct Comparator
      {
        bool operator()(const TabuListItem &li1, const TabuListItem &li2) const
        {
          return li1.tenure > li2.tenure;
        }
      };
    };
    
    template <class Queue>
    class QueueAdapter : public Queue
    {
    public:
      typedef typename Queue::container_type container_type;
      const container_type &operator*() const { return this->c; }
      container_type &operator*() { return this->c; }
    };
    
    /** The Tabu Search runner explores a subset of the current
     neighborhood. Among the elements in it, the one that gives the
     minimum value of the cost function becomes the new current
     state, independently of the fact whether its value is less or
     greater than the current one.
     
     Such a choice allows the algorithm to escape from local minima,
     but creates the risk of cycling among a set of states.  In order to
     prevent cycling, the so-called tabu list is used, which
     determines the forbidden moves. This list stores the most recently
     accepted moves, and the inverses of the moves in the list are
     forbidden.
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class TabuSearch : public MoveRunner<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      /** Constructor.
       @param in the Input object
       @param sm the State Manager
       @param ne the Neighborhood Explorer
       @param name the name of the runner
       */
      TabuSearch(StateManager& sm,
                 NeighborhoodExplorer& ne,
                 const std::string& name,
                 const std::string& description)
      : MoveRunner<StateManager, NeighborhoodExplorer>(sm, ne, name, description)
      {}
      
      ENABLE_RUNNER_CLONE()
      
      void Print(std::ostream &os) const override
      {
        Runner<Input, State, CostStructure>::Print(os);
        os << "{";
        size_t i = 0;
        for (typename PriorityQueue::container_type::const_iterator it = (*tabu_list).begin(); it != (*tabu_list).end(); ++it)
        {
          if (i > 0)
            os << ", ";
          os << it->move << "(" << it->tenure << ")";
          i++;
        }
        os << "}";
      }
      
    protected:
      bool MaxIdleIterationExpired() const
      {
        return this->iteration - this->iteration_of_best >= this->max_idle_iterations;
      }
      
      void InitializeRun(const Input& in) override
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::InitializeRun(in);
        (*tabu_list).clear();
      }
      
      
      /**
       The stop criterion is based on the number of iterations elapsed from
       the last strict improvement of the best state cost.
       */
      bool StopCriterion() const override
      {
        return MaxIdleIterationExpired() || this->MaxEvaluationsExpired();
      }
      
      /**
       Selects always the best move that is non prohibited by the tabu list
       mechanism.
       */
      void SelectMove(const Input& in) override
      {
        CostStructure aspiration = this->best_state_cost - this->current_state_cost;
        size_t explored;
        EvaluatedMove em = this->ne.SelectBest(in, *this->p_current_state, explored, [this, in, aspiration](const Move &mv, const CostStructure &move_cost) {
          for (auto li : *(this->tabu_list))
            if ((move_cost >= aspiration) && this->Inverse(in, *this->p_current_state, li.move, mv))
              return false;
          return true;
        },
                                               this->weights);
        this->current_move = em;
        this->evaluations += explored;
      }
      
      bool Inverse(const Input& in, const State& state, const Move& mv1, const Move& mv2) const
      {
        if (inverse_func == nullptr)
        {
          return mv1 == mv2;
        }
        if (inverse_func->first == std::type_index(typeid(InverseInputStateFuncType)))
          return boost::any_cast<InverseInputStateFuncType>(inverse_func->second)(in, state, mv1, mv2);
        else
        {
          assert(inverse_func->first == std::type_index(typeid(InverseFuncType)));
          return boost::any_cast<InverseFuncType>(inverse_func->second)(mv1, mv2);
        }
      }
      
      /**
       Stores the move by inserting it in the tabu list, if the state obtained
       is better than the one found so far also the best state is updated.
       */
      void CompleteMove(const Input& in) override
      {
        // remove no more tabu moves
        while (!tabu_list.empty() && tabu_list.top().tenure < this->iteration)
          tabu_list.pop();
        // insert current move
        tabu_list.emplace(this->current_move.move, this->iteration + Random::Uniform<unsigned int>(min_tenure, max_tenure));
      }
      
      void InitializeParameters() override
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::InitializeParameters();
        max_idle_iterations("max_idle_iterations", "Maximum number of idle iterations", this->parameters);
        min_tenure("min_tenure", "Minimum tabu tenure", this->parameters);
        max_tenure("max_tenure", "Maximum tabu tenure", this->parameters);
      }
      
    protected:
      using InverseMovesFunc = std::pair<std::type_index, boost::any>;
      
      using InverseFuncType = std::function<bool(const Move&, const Move&)>;
      using InverseInputStateFuncType = std::function<bool(const Input&, const State&, const Move&, const Move&)>;
      
    public:
      void SetInverseFunction(InverseFuncType&& r)
      {
        inverse_func = std::make_shared<InverseMovesFunc>(std::type_index(typeid(InverseFuncType)), r);
      }
      
      void SetInverseFunction(InverseInputStateFuncType&& r)
      {
        inverse_func = std::make_shared<InverseMovesFunc>(std::type_index(typeid(InverseInputStateFuncType)), r);
      }
      
      void SetInverseFunction(const InverseFuncType& r)
      {
        inverse_func = std::make_shared<InverseMovesFunc>(std::type_index(typeid(InverseFuncType)), r);
      }
      
      void SetInverseFunction(const InverseInputStateFuncType& r)
      {
        inverse_func = std::make_shared<InverseMovesFunc>(std::type_index(typeid(InverseInputStateFuncType)), r);
      }
      
    protected:
      std::shared_ptr<InverseMovesFunc> inverse_func;
            
      typedef QueueAdapter<std::priority_queue<TabuListItem<Move>, std::vector<TabuListItem<Move>>, typename TabuListItem<Move>::Comparator>> PriorityQueue;
      
      PriorityQueue tabu_list;
      // parameters
      Parameter<unsigned long int> max_idle_iterations;
      Parameter<unsigned int> min_tenure, max_tenure;
    };
  } // namespace Core
} // namespace EasyLocal
