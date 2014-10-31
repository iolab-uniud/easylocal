//
//  parallelneighborhoodexplorer.hh
//  EasyLocalExamples
//
//  Created by Luca Di Gaspero on 29/10/14.
//
//

#if defined(TBB_AVAILABLE)

#ifndef _parallelneighborhoodexplorer_hh
#define _parallelneighborhoodexplorer_hh

#include "easylocal/helpers/neighborhoodexplorer.hh"
#include <iterator>
#include <tbb/tbb.h>

namespace EasyLocal {
  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype = int>
    class NeighborhoodExplorerIteratorInterface;
    
    
    template <class Move, typename CFtype>
    struct ComputedMove
    {
      ComputedMove(const Move& move, CFtype cost) : move(move), cost(cost) {}
      ComputedMove() : cost(0) {}
      Move move;
      CFtype cost;
    };
    
    template <class Input, class State, class Move, typename CFtype>
    class NeighborhoodIterator : public std::iterator<std::input_iterator_tag, ComputedMove<Move, CFtype>>
    {
      friend class NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>;
    public:
      NeighborhoodIterator operator++(int) // postfix
      {
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !nhe.NextMove(state, current_move);
        move_count++;
        computed_move = ComputedMove<Move, CFtype>(current_move, 0);
        return *this;
      }
      NeighborhoodIterator operator++() // prefix
      {
        NeighborhoodIterator ni = *this;
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !nhe.NextMove(state, current_move);
        move_count++;
        computed_move = ComputedMove<Move, CFtype>(current_move, 0);
        return ni;
      }
      ComputedMove<Move, CFtype> operator*() const
      {
        return computed_move;
      }
      ComputedMove<Move, CFtype>* operator->() const
      {
        return &computed_move;
      }
      bool operator==(NeighborhoodIterator<Input, State, Move, CFtype> it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
      }
      bool operator!=(const NeighborhoodIterator<Input, State, Move, CFtype>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
      }
    protected:
      NeighborhoodIterator<Input, State, Move, CFtype>(const NeighborhoodExplorer<Input, State, Move, CFtype>& nhe, const State& state, bool end = false)
      : nhe(nhe), state(state), move_count(0), end(end)
      {
        if (end)
          return;
        try
        {
          nhe.FirstMove(state, current_move);
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
        computed_move = ComputedMove<Move, CFtype>(current_move, 0);
      }
      const NeighborhoodExplorer<Input, State, Move, CFtype>& nhe;
      const State& state;
      Move current_move;
      ComputedMove<Move, CFtype> computed_move;
      size_t move_count;
      bool end;
    };
    
    template <class Input, class State, class Move, typename CFtype>
    class NeighborhoodExplorerIteratorInterface
    {
    protected:
      static NeighborhoodIterator<Input, State, Move, CFtype> create_iterator(const NeighborhoodExplorer<Input, State, Move, CFtype>& nhe, const State& st, bool end = false)
      {
        return NeighborhoodIterator<Input, State, Move, CFtype>(nhe, st, end);
      }
    };
    
    template <class Input, class State, class Move, typename CFtype, class NHE>
    class ParallelNeighborhoodExplorer : public NHE, public NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>
    {
    public:
      using NHE::NHE;
    protected:
      NeighborhoodIterator<Input, State, Move, CFtype> begin(const State& st) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>::create_iterator(*this, st);
      }
      
      NeighborhoodIterator<Input, State, Move, CFtype> end(const State& st) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>::create_iterator(*this, st, true);
      }
      
      mutable tbb::concurrent_vector<ComputedMove<Move, CFtype>> moves;
      
      void FillMoveVector(const State& st) const
      {
        moves.reserve(moves.capacity());
        moves.resize(0);
        moves.grow_by(this->begin(st), this->end(st));
        if (moves.size() > 0)
          tbb::parallel_for<size_t>(0, moves.size() - 1, [this,&st](size_t i) {
            this->moves[i].cost = this->DeltaCostFunction(st, this->moves[i].move);
          });
        tbb::parallel_sort(moves.begin(), moves.end(), [](const ComputedMove<Move, CFtype>& cm1, const ComputedMove<Move, CFtype>& cm2)->bool {
          return cm1.cost < cm2.cost;
        });
      }
    public:
      virtual CFtype BestMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
      {
        FillMoveVector(st);
        if (moves.empty())
        {
          throw EmptyNeighborhood();
        }
        // Draw the best move randomly from the equally best moves
        CFtype best_cost = moves[0].cost;
        auto it = std::find_if(moves.begin(), moves.end(), [best_cost](const ComputedMove<Move, CFtype>& cm)->bool {
          return cm.cost > best_cost;
        });
        size_t number_of_bests = (it - moves.begin()) - 1;
        size_t i = Random::Int(0, number_of_bests - 1);
        mv = moves[i].move;
        return moves[i].cost;
      }
      
      virtual CFtype FirstImprovingMove(const State &st, Move& mv) const throw (EmptyNeighborhood)
      {
        ComputedMove<Move, CFtype> first_improving_move;
        bool first_improving_move_found = false;
        tbb::spin_mutex mx_first_improving_move;
        moves.reserve(moves.capacity());
        moves.resize(0);
        tbb::parallel_for_each(this->begin(st), this->end(st), [this,&st,&mx_first_improving_move,&first_improving_move,&first_improving_move_found](ComputedMove<Move, CFtype>& cm) {
          cm.cost = this->DeltaCostFunction(st, cm.move);
          moves.push_back(cm);
          if (cm.cost < 0)
          {
            tbb::spin_mutex::scoped_lock lock(mx_first_improving_move);
            first_improving_move_found = true;
            first_improving_move = cm;
            tbb::task::self().cancel_group_execution();
          }
        });
        if (!first_improving_move_found && moves.empty())
        {
          throw EmptyNeighborhood();
        }
        if (first_improving_move_found)
        {
          mv = first_improving_move.move;
          return first_improving_move.cost;
        }
        else
        {
          tbb::parallel_sort(moves.begin(), moves.end(), [](const ComputedMove<Move, CFtype>& cm1, const ComputedMove<Move, CFtype>& cm2)->bool {
            return cm1.cost < cm2.cost;
          });
          mv = moves[0].move;
          return moves[0].cost;
        }
      }
    };
  }
}

#endif

#endif
