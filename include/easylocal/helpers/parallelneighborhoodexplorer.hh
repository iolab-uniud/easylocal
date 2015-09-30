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
    
    template <class Input, class State, class Move, typename CFtype = int, class CostStructure = DefaultCostStructure<CFtype>>
    class NeighborhoodExplorerIteratorInterface;        
    
    template <class Input, class State, class Move, typename CFtype, class CostStructure>
    class FullNeighborhoodIterator : public std::iterator<std::input_iterator_tag, EvaluatedMove<Move, CFtype>>
    {
      friend class NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype, CostStructure>;
    public:
      FullNeighborhoodIterator operator++(int) // postfix
      {
        FullNeighborhoodIterator pi = *this;
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !ne.NextMove(state, current_move);
        move_count++;
        computed_move = EvaluatedMove<Move, CFtype, CostStructure>(current_move);
        return pi;
      }
      FullNeighborhoodIterator& operator++() // prefix
      {
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !ne.NextMove(state, current_move);
        move_count++;
        computed_move = EvaluatedMove<Move, CFtype, CostStructure>(current_move);
        return *this;
      }
      EvaluatedMove<Move, CFtype> operator*() const
      {
        return computed_move;
      }
      EvaluatedMove<Move, CFtype>* operator->() const
      {
        return &computed_move;
      }
      bool operator==(const FullNeighborhoodIterator<Input, State, Move, CFtype, CostStructure>& it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
      }
      bool operator!=(const FullNeighborhoodIterator<Input, State, Move, CFtype, CostStructure>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
      }
    protected:
      FullNeighborhoodIterator(const NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& ne, const State& state, bool end = false)
      : ne(ne), state(state), move_count(0), end(end)
      {
        if (end)
          return;
        try
        {
          ne.FirstMove(state, current_move);
          computed_move = EvaluatedMove<Move, CFtype, CostStructure>(current_move);
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
      }
      const NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& ne;
      const State& state;
      Move current_move;
      EvaluatedMove<Move, CFtype, CostStructure> computed_move;
      size_t move_count;
      bool end;
    };
    
    template <class Input, class State, class Move, typename CFtype, class CostStructure>
    class SampleNeighborhoodIterator : public std::iterator<std::input_iterator_tag, EvaluatedMove<Move, CFtype>>
    {
      friend class NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype, CostStructure>;
    public:
      SampleNeighborhoodIterator operator++(int) // postfix
      {
        SampleNeighborhoodIterator pi = *this;
        if (end)
          throw std::logic_error("Attempting to go after last move");
        move_count++;
        end = move_count >= samples;
        if (!end)
        {
          ne.RandomMove(state, current_move);
          computed_move = EvaluatedMove<Move, CFtype, CostStructure>(current_move, 0);
        }
        return pi;
      }
      SampleNeighborhoodIterator& operator++() // prefix
      {
        if (end)
          throw std::logic_error("Attempting to go after last move");
        move_count++;
        end = move_count >= samples;
        if (!end)
        {
          ne.RandomMove(state, current_move);
          computed_move = EvaluatedMove<Move, CFtype, CostStructure>(current_move);
        }
        return *this;
      }
      EvaluatedMove<Move, CFtype> operator*() const
      {
        return computed_move;
      }
      const EvaluatedMove<Move, CFtype>* operator->() const
      {
        return &computed_move;
      }
      bool operator==(const SampleNeighborhoodIterator<Input, State, Move, CFtype, CostStructure>& it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
      }
      bool operator!=(const SampleNeighborhoodIterator<Input, State, Move, CFtype, CostStructure>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
      }
    protected:
      SampleNeighborhoodIterator(const NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& ne, const State& state, size_t samples, bool end = false)
      : ne(ne), state(state), move_count(0), samples(samples), end(end)
      {
        if (end)
          return;
        try
        {
          ne.RandomMove(state, current_move);
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
        computed_move = EvaluatedMove<Move, CFtype, CostStructure>(current_move);
      }
      const NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& ne;
      const State& state;
      Move current_move;
      EvaluatedMove<Move, CFtype, CostStructure> computed_move;
      size_t move_count, samples;
      bool end;
    };
    
    template <class Input, class State, class Move, typename CFtype, class CostStructure>
    class NeighborhoodExplorerIteratorInterface
    {
    protected:
      static FullNeighborhoodIterator<Input, State, Move, CFtype, CostStructure> create_full_neighborhood_iterator(const NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& ne, const State& st, bool end = false)
      {
        return FullNeighborhoodIterator<Input, State, Move, CFtype, CostStructure>(ne, st, end);
      }
      
      static SampleNeighborhoodIterator<Input, State, Move, CFtype, CostStructure> create_sample_neighborhood_iterator(const NeighborhoodExplorer<Input, State, Move, CFtype, CostStructure>& ne, const State& st, size_t samples, bool end = false)
      {
        return SampleNeighborhoodIterator<Input, State, Move, CFtype, CostStructure>(ne, st, samples, end);
      }
    };
    
    template <class Input, class State, class NE>
    class ParallelNeighborhoodExplorer : public NE, public NeighborhoodExplorerIteratorInterface<Input, State, typename NE::MoveType, typename NE::CostType, typename NE::CostStructureType>
    {
    public:
      using NE::NE;
      using typename NE::MoveAcceptor;
      using typename NE::MoveType;
      using typename NE::CostType;
      using typename NE::CostStructureType;
    protected:
      FullNeighborhoodIterator<Input, State, MoveType, CostType, CostStructureType> begin(const State& st) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, MoveType, CostType, CostStructureType>::create_full_neighborhood_iterator(*this, st);
      }
      
      FullNeighborhoodIterator<Input, State, MoveType, CostType, CostStructureType> end(const State& st) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, MoveType, CostType, CostStructureType>::create_full_neighborhood_iterator(*this, st, true);
      }
      
      SampleNeighborhoodIterator<Input, State, MoveType, CostType, CostStructureType> sample_begin(const State& st, size_t samples) const
      {
        if (samples > 0)
          return NeighborhoodExplorerIteratorInterface<Input, State, MoveType, CostType, CostStructureType>::create_sample_neighborhood_iterator(*this, st, samples);
        else
          return sample_end(st, samples);
      }
      
      SampleNeighborhoodIterator<Input, State, MoveType, CostType, CostStructureType> sample_end(const State& st, size_t samples) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, MoveType, CostType, CostStructureType>::create_sample_neighborhood_iterator(*this, st, samples, true);
      }
    public:
      virtual EvaluatedMove<MoveType, CostType, CostStructureType> SelectFirst(const State &st, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        EvaluatedMove<MoveType, CostType, CostStructureType> first_move;
        bool first_move_found = false;
        tbb::spin_mutex mx_first_move;
        explored = 0;
        tbb::parallel_for_each(this->begin(st), this->end(st), [this, &st, &mx_first_move, &first_move, &first_move_found, AcceptMove, &weights, &explored](EvaluatedMove<MoveType, CostType, CostStructureType>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          tbb::spin_mutex::scoped_lock lock(mx_first_move);
          explored++;
          if (!first_move_found)
          {
            if (AcceptMove(mv.move, mv.cost))
            {
              first_move_found = true;
              first_move = mv;
              tbb::task::self().cancel_group_execution();
            }
          }
        });
        if (!first_move_found)
          return EvaluatedMove<MoveType, CostType, CostStructureType>::empty;
        return first_move;
      }
      
      virtual EvaluatedMove<MoveType, CostType, CostStructureType> SelectBest(const State &st, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        tbb::spin_mutex mx_best_move;
        EvaluatedMove<MoveType, CostType, CostStructureType> best_move;
        unsigned int number_of_bests = 0;
        explored = 0;
        tbb::parallel_for_each(this->begin(st), this->end(st), [this, &st, &mx_best_move, &best_move, &number_of_bests, AcceptMove, &weights, &explored](EvaluatedMove<MoveType, CostType, CostStructureType>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          tbb::spin_mutex::scoped_lock lock(mx_best_move);
          explored++;
          if (AcceptMove(mv.move, mv.cost))
          {
            if (number_of_bests == 0)
            {
              best_move = mv;
              number_of_bests = 1;
            }
            else if (mv.cost < best_move.cost)
            {
              best_move = mv;
              number_of_bests = 1;
            }
            else if (mv.cost == best_move.cost)
            {
              if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
                best_move = mv;
              number_of_bests++;
            }
          }
        });
        if (number_of_bests == 0)
          return EvaluatedMove<MoveType, CostType, CostStructureType>::empty;
        return best_move;
      }
      
      virtual EvaluatedMove<MoveType, CostType, CostStructureType> RandomFirst(const State &st, size_t samples, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        EvaluatedMove<MoveType, CostType, CostStructureType> first_move;
        bool first_move_found = false;
        tbb::spin_mutex mx_first_move;
        explored = 0;
        tbb::parallel_for_each(this->sample_begin(st, samples), this->sample_end(st, samples), [this, &st, &mx_first_move, &first_move, &first_move_found, &explored, AcceptMove, &weights](EvaluatedMove<MoveType, CostType, CostStructureType>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          tbb::spin_mutex::scoped_lock lock(mx_first_move);
          explored++;
          if (!first_move_found && AcceptMove(mv.move, mv.cost))
          {
            first_move_found = true;
            first_move = mv;
            tbb::task::self().cancel_group_execution();
          }
        });
        if (!first_move_found)
          return EvaluatedMove<MoveType, CostType, CostStructureType>::empty;
        return first_move;
      }
      
      virtual EvaluatedMove<MoveType, CostType, CostStructureType> RandomBest(const State &st, size_t samples, size_t& explored, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        tbb::spin_mutex mx_best_move;
        EvaluatedMove<MoveType, CostType, CostStructureType> best_move;
        unsigned int number_of_bests = 0;
        explored = 0;
        tbb::parallel_for_each(this->sample_begin(st, samples), this->sample_end(st, samples), [this, &st, &mx_best_move, &best_move, &number_of_bests, &explored, AcceptMove, &weights](EvaluatedMove<MoveType, CostType, CostStructureType>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          tbb::spin_mutex::scoped_lock lock(mx_best_move);
          explored++;
          if (AcceptMove(mv.move, mv.cost))
          {
            if (number_of_bests == 0)
            {
              best_move = mv;
              number_of_bests = 1;
            }
            else if (mv.cost < best_move.cost)
            {
              best_move = mv;
              number_of_bests = 1;
            }
            else if (mv.cost == best_move.cost)
            {
              if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
                best_move = mv;
              number_of_bests++;
            }
          }
        });
        if (number_of_bests == 0)
          return EvaluatedMove<MoveType, CostType, CostStructureType>::empty;
        return best_move;
      }
    };
  }
}

#endif

#endif
