#if defined(TBB_AVAILABLE)
#pragma once

#include "helpers/neighborhoodexplorer.hh"
#include <iterator>
#include <tbb/tbb.h>

namespace EasyLocal
{
  namespace Core
  {
    
    template <class NeighborhoodExplorer>
    class NeighborhoodExplorerIteratorInterface;
    
    
    template <class NeighborhoodExplorer>
    class FullNeighborhoodIterator : public std::iterator<std::input_iterator_tag, typename NeighborhoodExplorer::EvaluatedMove>
    {
      friend class NeighborhoodExplorerIteratorInterface<NeighborhoodExplorer>;
    protected:
      using Input = typename NeighborhoodExplorer::Input;
      using State = typename NeighborhoodExplorer::State;
      using Move = typename NeighborhoodExplorer::Move;
      using EvaluatedMove = typename NeighborhoodExplorer::EvaluatedMove;
      using CostStructure = typename NeighborhoodExplorer::CostStructure;
    public:
      FullNeighborhoodIterator operator++(int) // postfix
      {
        FullNeighborhoodIterator pi = *this;
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !ne.NextMove(state, current_move);
        move_count++;
        computed_move = EvaluatedMove(current_move);
        return pi;
      }
      
      FullNeighborhoodIterator &operator++() // prefix
      {
        if (end)
          throw std::logic_error("Attempting to go after last move");
        end = !ne.NextMove(in, state, current_move);
        move_count++;
        computed_move = EvaluatedMove(current_move);
        return *this;
      }
      
      EvaluatedMove operator*() const
      {
        return computed_move;
      }
      
      EvaluatedMove *operator->() const
      {
        return &computed_move;
      }
      
      bool operator==(const FullNeighborhoodIterator<NeighborhoodExplorer> &it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
      }
      
      bool operator!=(const FullNeighborhoodIterator<NeighborhoodExplorer> &it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
      }
      
    protected:
      FullNeighborhoodIterator(const NeighborhoodExplorer &ne, const Input& in, const State &state, bool end = false)
      : ne(ne), in(in), state(state), move_count(0), end(end)
      {
        if (end)
          return;
        try
        {
          ne.FirstMove(in, state, current_move);
          computed_move = EvaluatedMove(current_move);
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
      }
      const NeighborhoodExplorer &ne;
      const Input &in;
      const State &state;
      Move current_move;
      EvaluatedMove computed_move;
      size_t move_count;
      bool end;
    };
    
    template <class NeighborhoodExplorer>
    class SampleNeighborhoodIterator : public std::iterator<std::input_iterator_tag, typename NeighborhoodExplorer::EvaluatedMove>
    {
      friend class NeighborhoodExplorerIteratorInterface<NeighborhoodExplorer>;
    protected:
      using Input = typename NeighborhoodExplorer::Input;
      using State = typename NeighborhoodExplorer::State;
      using Move = typename NeighborhoodExplorer::Move;
      using EvaluatedMove = typename NeighborhoodExplorer::EvaluatedMove;
      using CostStructure = typename NeighborhoodExplorer::CostStructure;
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
          ne.RandomMove(in, state, current_move);
          computed_move = EvaluatedMove(current_move, 0);
        }
        return pi;
      }
      SampleNeighborhoodIterator &operator++() // prefix
      {
        if (end)
          throw std::logic_error("Attempting to go after last move");
        move_count++;
        end = move_count >= samples;
        if (!end)
        {
          ne.RandomMove(in, state, current_move);
          computed_move = EvaluatedMove(current_move);
        }
        return *this;
      }
      EvaluatedMove operator*() const
      {
        return computed_move;
      }
      const EvaluatedMove *operator->() const
      {
        return &computed_move;
      }
      bool operator==(const SampleNeighborhoodIterator<NeighborhoodExplorer> &it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
      }
      bool operator!=(const SampleNeighborhoodIterator<NeighborhoodExplorer> &it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
      }
      
    protected:
      SampleNeighborhoodIterator(const NeighborhoodExplorer &ne, const Input& in, const State &state, size_t samples, bool end = false)
      : ne(ne), in(in), state(state), move_count(0), samples(samples), end(end)
      {
        if (end)
          return;
        try
        {
          ne.RandomMove(in, state, current_move);
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
        computed_move = EvaluatedMove(current_move);
      }
      const NeighborhoodExplorer &ne;
      const Input &in;
      const State &state;
      Move current_move;
      EvaluatedMove computed_move;
      size_t move_count, samples;
      bool end;
    };
    
    template <class NeighborhoodExplorer>
    class NeighborhoodExplorerIteratorInterface
    {
    protected:
      using Input = typename NeighborhoodExplorer::Input;
      using State = typename NeighborhoodExplorer::State;
      using Move = typename NeighborhoodExplorer::Move;
      using CostStructure = typename NeighborhoodExplorer::CostStructure;
      
      static FullNeighborhoodIterator<NeighborhoodExplorer> create_full_neighborhood_iterator(const NeighborhoodExplorer&ne, const Input& in, const State &st, bool end = false)
      {
        return FullNeighborhoodIterator<NeighborhoodExplorer>(ne, in, st, end);
      }
      
      static SampleNeighborhoodIterator<NeighborhoodExplorer> create_sample_neighborhood_iterator(const NeighborhoodExplorer&ne, const Input& in, const State &st, size_t samples, bool end = false)
      {
        return SampleNeighborhoodIterator<NeighborhoodExplorer>(ne, in, st, samples, end);
      }
    };
    
    template <class StateManager, class NeighborhoodExplorer>
    class ParallelNeighborhoodExplorer : public NeighborhoodExplorer, public NeighborhoodExplorerIteratorInterface<NeighborhoodExplorer>
    {
    public:
      using Input = typename NeighborhoodExplorer::Input;
      using State = typename NeighborhoodExplorer::State;
      using Move = typename NeighborhoodExplorer::Move;
      using EvaluatedMove = typename NeighborhoodExplorer::EvaluatedMove;
      using CostStructure = typename NeighborhoodExplorer::CostStructure;
      using MoveAcceptor = typename NeighborhoodExplorer::MoveAcceptor;
      
      using NeighborhoodExplorer::NeighborhoodExplorer;
      
    protected:
      FullNeighborhoodIterator<NeighborhoodExplorer> begin(const Input& in, const State &st) const
      {
        return NeighborhoodExplorerIteratorInterface<NeighborhoodExplorer>::create_full_neighborhood_iterator(*this, in, st);
      }
      
      FullNeighborhoodIterator<NeighborhoodExplorer> end(const Input& in, const State &st) const
      {
        return NeighborhoodExplorerIteratorInterface<NeighborhoodExplorer>::create_full_neighborhood_iterator(*this, in, st, true);
      }
      
      SampleNeighborhoodIterator<NeighborhoodExplorer> sample_begin(const Input& in, const State &st, size_t samples) const
      {
        if (samples > 0)
          return NeighborhoodExplorerIteratorInterface<NeighborhoodExplorer>::create_sample_neighborhood_iterator(*this, in, st, samples);
        else
          return sample_end(in, st, samples);
      }
      
      SampleNeighborhoodIterator<NeighborhoodExplorer> sample_end(const Input& in, const State &st, size_t samples) const
      {
        return NeighborhoodExplorerIteratorInterface<NeighborhoodExplorer>::create_sample_neighborhood_iterator(*this, in, st, samples, true);
      }
      
    public:
      virtual EvaluatedMove SelectFirst(const Input& in, const State &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        EvaluatedMove first_move;
        bool first_move_found = false;
        tbb::spin_mutex mx_first_move;
        explored = 0;
        tbb::parallel_for_each(this->begin(in, st), this->end(in, st), [this, &in, &st, &mx_first_move, &first_move, &first_move_found, AcceptMove, &weights, &explored](EvaluatedMove& mv) {
          mv.cost = this->DeltaCostFunctionComponents(in, st, mv.move, weights);
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
          return EvaluatedMove::empty;
        return first_move;
      }
      
      virtual EvaluatedMove SelectBest(const Input& in, const State &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        tbb::spin_mutex mx_best_move;
        EvaluatedMove best_move;
        unsigned int number_of_bests = 0;
        explored = 0;
        tbb::parallel_for_each(this->begin(in, st), this->end(in, st), [this, &in, &st, &mx_best_move, &best_move, &number_of_bests, AcceptMove, &weights, &explored](EvaluatedMove &mv) {
          mv.cost = this->DeltaCostFunctionComponents(in, st, mv.move, weights);
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
              if (Random::Uniform<unsigned int>(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
                best_move = mv;
              number_of_bests++;
            }
          }
        });
        if (number_of_bests == 0)
          return EvaluatedMove::empty;
        return best_move;
      }
      
      virtual EvaluatedMove RandomFirst(const Input& in, const State &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        EvaluatedMove first_move;
        bool first_move_found = false;
        tbb::spin_mutex mx_first_move;
        explored = 0;
        tbb::parallel_for_each(this->sample_begin(in, st, samples), this->sample_end(in, st, samples), [this, &in, &st, &mx_first_move, &first_move, &first_move_found, &explored, AcceptMove, &weights](EvaluatedMove &mv) {
          mv.cost = this->DeltaCostFunctionComponents(in, st, mv.move, weights);
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
          return EvaluatedMove::empty;
        return first_move;
      }
      
      virtual EvaluatedMove RandomBest(const Input& in, const State &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        tbb::spin_mutex mx_best_move;
        EvaluatedMove best_move;
        unsigned int number_of_bests = 0;
        explored = 0;
        tbb::parallel_for_each(this->sample_begin(in, st, samples), this->sample_end(in, st, samples), [this, &in, &st, &mx_best_move, &best_move, &number_of_bests, &explored, AcceptMove, &weights](EvaluatedMove &mv) {
          mv.cost = this->DeltaCostFunctionComponents(in, st, mv.move, weights);
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
              if (Random::Uniform<unsigned int>(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
                best_move = mv;
              number_of_bests++;
            }
          }
        });
        if (number_of_bests == 0)
          return EvaluatedMove::empty;
        return best_move;
      }
    };
  } // namespace Core
} // namespace EasyLocal

#endif
