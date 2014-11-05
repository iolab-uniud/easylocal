#if !defined(_TABU_SEARCH_HH_)
#define _TABU_SEARCH_HH_

#include <stdexcept>
#include <list>
#include <queue>

#include "easylocal/runners/moverunner.hh"
#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {
  
  namespace Core {
    
    template <class Move>
    struct TabuListItem
    {
      TabuListItem() : tenure(0) {}
      TabuListItem(const Move& move, unsigned long int tenure) : move(move), tenure(tenure) {}
      Move move;
      unsigned long int tenure;
      
      struct Comparator
      {
        bool operator()(const TabuListItem& li1, const TabuListItem& li2) const
        {
          return li1.tenure > li2.tenure;
        }
      };
    };
    
    template <class Queue>
    class QueueAdapter : public Queue {
    public:
      typedef typename Queue::container_type container_type;
      const container_type& operator*() const { return this->c; }
      container_type& operator*() { return this->c; }
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
    template <class Input, class State, class Move, typename CFtype = int>
    class TabuSearch : public MoveRunner<Input, State, Move, CFtype>
    {
    public:
      typedef std::function<bool(const Move& lm, const Move& mv)> InverseFunction;
      
      TabuSearch(const Input& in, StateManager<Input, State, CFtype>& sm,
                 NeighborhoodExplorer<Input, State, Move, CFtype>& ne,
                 std::string name,
                 const InverseFunction& Inverse = SameMoveAsInverse);
      std::string StatusString();
      
      virtual void Print(std::ostream& os = std::cout) const;

    protected:
      bool MaxIdleIterationExpired() const;
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      bool StopCriterion();
      void SelectMove();
      void CompleteMove();
      void RegisterParameters();
      const InverseFunction& Inverse;
      
      static InverseFunction SameMoveAsInverse;
            
      typedef QueueAdapter<std::priority_queue<TabuListItem<Move>, std::vector<TabuListItem<Move>>, typename TabuListItem<Move>::Comparator>> PriorityQueue;
      
      PriorityQueue tabu_list;
      // parameters
      Parameter<unsigned long int> max_idle_iterations;
      Parameter<unsigned int> min_tenure, max_tenure;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a tabu search runner by linking it to a state manager,
     a neighborhood explorer, a tabu list manager, and an input object.
     
     @param s a pointer to a compatible state manager
     @param ne a pointer to a compatible neighborhood explorer
     @param tlm a pointer to a compatible tabu list manager
     @param in a poiter to an input object
     */
    
    template <class Input, class State, class Move, typename CFtype>
    TabuSearch<Input, State, Move, CFtype>::TabuSearch(const Input& in,
                                                       StateManager<Input, State, CFtype>& sm,
                                                       NeighborhoodExplorer<Input, State, Move, CFtype>& ne,
                                                       std::string name,
                                                       const InverseFunction& Inverse)
    : MoveRunner<Input, State, Move, CFtype>(in, sm, ne, name, "Tabu Search Runner"), Inverse(Inverse)
    {}
    
    
    template <class Input, class State, class Move, typename CFtype>
    void TabuSearch<Input, State, Move, CFtype>::RegisterParameters()
    {
      MoveRunner<Input, State, Move, CFtype>::RegisterParameters();
      max_idle_iterations("max_idle_iterations", "Maximum number of idle iterations", this->parameters);
      min_tenure("min_tenure", "Minimum tabu tenure", this->parameters);
      max_tenure("max_tenure", "Maximum tabu tenure", this->parameters);
    }
    
    template <class Input, class State, class Move, typename CFtype>
    void TabuSearch<Input, State, Move, CFtype>::Print(std::ostream& os) const
    {
      Runner<Input, State, CFtype>::Print(os);
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
    
    /**
     Initializes the run by invoking the companion superclass method, and
     cleans the tabu list.
     */
    template <class Input, class State, class Move, typename CFtype>
    void TabuSearch<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      MoveRunner<Input, State, Move, CFtype>::InitializeRun();
      (*tabu_list).clear();
    }
    
    
    /**
     Selects always the best move that is non prohibited by the tabu list
     mechanism.
     */
    template <class Input, class State, class Move, typename CFtype>
    void TabuSearch<Input, State, Move, CFtype>::SelectMove()
    {
      CFtype aspiration = this->best_state_cost.total - this->current_state_cost.total;
      EvaluatedMove<Move, CFtype> em = this->ne.SelectBest(*this->p_current_state, [this, aspiration](const Move& mv, CostStructure<CFtype> move_cost) {
        for (auto li : *(this->tabu_list))
          if (this->Inverse(li.move, mv) && (move_cost.total >= aspiration))
            return false;
        return true;
      }, this->weights);
      this->current_move = em;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    bool TabuSearch<Input, State, Move, CFtype>::MaxIdleIterationExpired() const
    {
      return this->iteration - this->iteration_of_best >= this->max_idle_iterations;
    }
    
    /**
     The stop criterion is based on the number of iterations elapsed from
     the last strict improvement of the best state cost.
     */
    template <class Input, class State, class Move, typename CFtype>
    bool TabuSearch<Input, State, Move, CFtype>::StopCriterion()
    {
      return MaxIdleIterationExpired() || this->MaxIterationExpired();
    }       
    
    /**
     Stores the move by inserting it in the tabu list, if the state obtained
     is better than the one found so far also the best state is updated.
     */
    template <class Input, class State, class Move, typename CFtype>
    void TabuSearch<Input, State, Move, CFtype>::CompleteMove()
    {
      std::cout << StatusString() << std::endl;
      
      // remove no more tabu moves
      while (!tabu_list.empty() && tabu_list.top().tenure < this->iteration)
        tabu_list.pop();
      // insert current move
      tabu_list.emplace(this->current_move.move, this->iteration + Random::Int(min_tenure, max_tenure));
    }
    
    /**
     Create a string containing the status of the runner
     */
    template <class Input, class State, class Move, typename CFtype>
    std::string TabuSearch<Input, State, Move, CFtype>::StatusString()
    {
      std::stringstream status;
      status << "TL = #" << tabu_list.size() << "[";
      size_t i = 0;
      for (auto li : (*tabu_list))
      {
        if (i > 0)
          status << ", ";
        status << li.move << "(" << li.tenure << ")";
        i++;
      }
      status << "]";
      return status.str();
    }  
  }
  
  template <class Input, class State, class Move, typename CFtype>
  typename TabuSearch<Input, State, Move, CFtype>::InverseFunction TabuSearch<Input, State, Move, CFtype>::SameMoveAsInverse = [](const Move& lm, const Move& om) { return lm == om; };
}

#endif // _TABU_SEARCH_HH_
