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
    
    template <class Input, class State, class Move, typename CFtype>
    class FullNeighborhoodIterator : public std::iterator<std::input_iterator_tag, EvaluatedMove<Move, CFtype>>
    {
      friend class NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>;
    public:
      FullNeighborhoodIterator operator++(int) // postfix
      {
        FullNeighborhoodIterator pi = *this;
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !ne.NextMove(state, current_move);
        move_count++;
        computed_move = EvaluatedMove<Move, CFtype>(current_move);
        return pi;
      }
      FullNeighborhoodIterator& operator++() // prefix
      {
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !ne.NextMove(state, current_move);
        move_count++;
        computed_move = EvaluatedMove<Move, CFtype>(current_move);
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
      bool operator==(FullNeighborhoodIterator<Input, State, Move, CFtype> it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
      }
      bool operator!=(const FullNeighborhoodIterator<Input, State, Move, CFtype>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
      }
    protected:
      FullNeighborhoodIterator<Input, State, Move, CFtype>(const NeighborhoodExplorer<Input, State, Move, CFtype>& ne, const State& state, bool end = false)
      : ne(ne), state(state), move_count(0), end(end)
      {
        if (end)
          return;
        try
        {
          ne.FirstMove(state, current_move);
          computed_move = EvaluatedMove<Move, CFtype>(current_move);
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
      }
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      const State& state;
      Move current_move;
      EvaluatedMove<Move, CFtype> computed_move;
      size_t move_count;
      bool end;
    };
    
    template <class Input, class State, class Move, typename CFtype>
    class SampleNeighborhoodIterator : public std::iterator<std::input_iterator_tag, EvaluatedMove<Move, CFtype>>
    {
      friend class NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>;
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
          computed_move = EvaluatedMove<Move, CFtype>(current_move, 0);
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
          computed_move = EvaluatedMove<Move, CFtype>(current_move);
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
      bool operator==(SampleNeighborhoodIterator<Input, State, Move, CFtype> it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
      }
      bool operator!=(const SampleNeighborhoodIterator<Input, State, Move, CFtype>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
      }
    protected:
      SampleNeighborhoodIterator<Input, State, Move, CFtype>(const NeighborhoodExplorer<Input, State, Move, CFtype>& ne, const State& state, size_t samples, bool end = false)
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
        computed_move = EvaluatedMove<Move, CFtype>(current_move);
      }
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      const State& state;
      Move current_move;
      EvaluatedMove<Move, CFtype> computed_move;
      size_t move_count, samples;
      bool end;
    };
    
    template <class Input, class State, class Move, typename CFtype>
    class NeighborhoodExplorerIteratorInterface
    {
    protected:
      static FullNeighborhoodIterator<Input, State, Move, CFtype> create_full_neighborhood_iterator(const NeighborhoodExplorer<Input, State, Move, CFtype>& ne, const State& st, bool end = false)
      {
        return FullNeighborhoodIterator<Input, State, Move, CFtype>(ne, st, end);
      }
      
      static SampleNeighborhoodIterator<Input, State, Move, CFtype> create_sample_neighborhood_iterator(const NeighborhoodExplorer<Input, State, Move, CFtype>& ne, const State& st, size_t samples, bool end = false)
      {
        return SampleNeighborhoodIterator<Input, State, Move, CFtype>(ne, st, samples, end);
      }
    };
    
    template <class Input, class State, class Move, typename CFtype, class ne>
    class ParallelNeighborhoodExplorer : public ne, public NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>
    {
    public:
      using ne::ne;
      using typename ne::MoveAcceptor;
    protected:
      FullNeighborhoodIterator<Input, State, Move, CFtype> begin(const State& st) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>::create_full_neighborhood_iterator(*this, st);
      }
      
      FullNeighborhoodIterator<Input, State, Move, CFtype> end(const State& st) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>::create_full_neighborhood_iterator(*this, st, true);
      }
      
      SampleNeighborhoodIterator<Input, State, Move, CFtype> sample_begin(const State& st, size_t samples) const
      {
        if (samples > 0)
          return NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>::create_sample_neighborhood_iterator(*this, st, samples);
        else
          return sample_end(st, samples);
      }
      
      SampleNeighborhoodIterator<Input, State, Move, CFtype> sample_end(const State& st, size_t samples) const
      {
        return NeighborhoodExplorerIteratorInterface<Input, State, Move, CFtype>::create_sample_neighborhood_iterator(*this, st, samples, true);
      }
    public:
      virtual EvaluatedMove<Move, CFtype> SelectFirst(const State &st, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        EvaluatedMove<Move, CFtype> first_move;
        bool first_move_found = false;
        tbb::spin_mutex mx_first_move;
        tbb::parallel_for_each(this->begin(st), this->end(st), [this, &st, &mx_first_move, &first_move, &first_move_found, AcceptMove, &weights](EvaluatedMove<Move, CFtype>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          tbb::spin_mutex::scoped_lock lock(mx_first_move);
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
          return EvaluatedMove<Move, CFtype>::empty;
        return first_move;
      }
      
      virtual EvaluatedMove<Move, CFtype> SelectBest(const State &st, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        tbb::spin_mutex mx_best_move;
        EvaluatedMove<Move, CFtype> best_move;
        unsigned int number_of_bests = 0;
        tbb::parallel_for_each(this->begin(st), this->end(st), [this, &st, &mx_best_move, &best_move, &number_of_bests, AcceptMove, &weights](EvaluatedMove<Move, CFtype>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          tbb::spin_mutex::scoped_lock lock(mx_best_move);
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
          return EvaluatedMove<Move, CFtype>::empty;
        return best_move;
      }
      
      virtual EvaluatedMove<Move, CFtype> RandomFirst(const State &st, size_t samples, size_t& sampled, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        EvaluatedMove<Move, CFtype> first_move;
        bool first_move_found = false;
        tbb::spin_mutex mx_first_move;
        tbb::atomic<size_t> c_sampled = 0;
        tbb::parallel_for_each(this->sample_begin(st, samples), this->sample_end(st, samples), [this, &st, &mx_first_move, &first_move, &first_move_found, &c_sampled, AcceptMove, &weights](EvaluatedMove<Move, CFtype>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          c_sampled++;
          tbb::spin_mutex::scoped_lock lock(mx_first_move);
          if (!first_move_found && AcceptMove(mv.move, mv.cost))
          {
            first_move_found = true;
            first_move = mv;
            tbb::task::self().cancel_group_execution();
          }
        });
        if (!first_move_found)
          return EvaluatedMove<Move, CFtype>::empty;
        sampled = c_sampled;
        return first_move;
      }
      
      virtual EvaluatedMove<Move, CFtype> RandomBest(const State &st, size_t samples, size_t& sampled, const MoveAcceptor& AcceptMove, const std::vector<double>& weights = std::vector<double>(0)) const throw (EmptyNeighborhood)
      {
        tbb::spin_mutex mx_best_move;
        EvaluatedMove<Move, CFtype> best_move;
        unsigned int number_of_bests = 0;
        tbb::atomic<size_t> c_sampled = 0;
        tbb::parallel_for_each(this->sample_begin(st, samples), this->sample_end(st, sampled), [this, &st, &mx_best_move, &best_move, &number_of_bests, &c_sampled, AcceptMove, &weights](EvaluatedMove<Move, CFtype>& mv) {
          mv.cost = this->DeltaCostFunctionComponents(st, mv.move, weights);
          mv.is_valid = true;
          c_sampled++;
          tbb::spin_mutex::scoped_lock lock(mx_best_move);
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
          return EvaluatedMove<Move, CFtype>::empty;
        sampled = c_sampled;
        return best_move;
      }
    };
  }
}

#endif

#endif
