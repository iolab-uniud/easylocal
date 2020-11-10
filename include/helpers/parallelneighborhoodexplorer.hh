#if defined(TBB_AVAILABLE)

#pragma once

#include "neighborhoodexplorer.hh"
#include <iterator>
#include <tbb/tbb.h>

namespace EasyLocal
{
namespace Core
{

template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
class NeighborhoodExplorerIteratorInterface;

template <class Input, class Solution, class Move, class CostStructure>
class FullNeighborhoodIterator : public std::iterator<std::input_iterator_tag, EvaluatedMove<Move, CostStructure>>
{
  friend class NeighborhoodExplorerIteratorInterface<Input, Solution, Move, CostStructure>;

public:
  FullNeighborhoodIterator operator++(int) // postfix
  {
    FullNeighborhoodIterator pi = *this;
    if (end)
      throw std::logic_error("Attempting to go after last move");
    end = !ne.NextMove(state, current_move);
    move_count++;
    computed_move = EvaluatedMove<Move, CostStructure>(current_move);
    return pi;
  }
  FullNeighborhoodIterator &operator++() // prefix
  {
    if (end)
      throw std::logic_error("Attempting to go after last move");
    end = !ne.NextMove(state, current_move);
    move_count++;
    computed_move = EvaluatedMove<Move, CostStructure>(current_move);
    return *this;
  }
  EvaluatedMove<Move, CostStructure> operator*() const
  {
    return computed_move;
  }
  EvaluatedMove<Move, CostStructure> *operator->() const
  {
    return &computed_move;
  }
  bool operator==(const FullNeighborhoodIterator<Input, Solution, Move, CostStructure> &it2) const
  {
    if (end && it2.end)
      return true;
    return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
  }
  bool operator!=(const FullNeighborhoodIterator<Input, Solution, Move, CostStructure> &it2)
  {
    if (end && it2.end)
      return false;
    return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
  }

protected:
  FullNeighborhoodIterator(const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne, const Solution &state, bool end = false)
      : ne(ne), state(state), move_count(0), end(end)
  {
    if (end)
      return;
    try
    {
      ne.FirstMove(state, current_move);
      computed_move = EvaluatedMove<Move, CostStructure>(current_move);
    }
    catch (EmptyNeighborhood)
    {
      end = true;
    }
  }
  const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne;
  const Solution &state;
  Move current_move;
  EvaluatedMove<Move, CostStructure> computed_move;
  size_t move_count;
  bool end;
};
  
  template <class Input, class Solution, class Move, class CostStructure>
  class StartingNeighborhoodIterator : public std::iterator<std::input_iterator_tag, EvaluatedMove<Move, CostStructure>>
  {
    friend class NeighborhoodExplorerIteratorInterface<Input, Solution, Move, CostStructure>;
    
  public:
    StartingNeighborhoodIterator operator++(int) // postfix
    {
      StartingNeighborhoodIterator pi = *this;
      if (end)
        throw std::logic_error("Attempting to go after last move");
      if (!ne.NextMove(state, current_move))
      {
        ne.FirstMove(state, current_move);
        rounds++;
      }
      end = (current_move == start_move) || rounds > 1;
      move_count++;
      computed_move = EvaluatedMove<Move, CostStructure>(current_move);
      return pi;
    }
    StartingNeighborhoodIterator &operator++() // prefix
    {
      if (end)
        throw std::logic_error("Attempting to go after last move");
      if (!ne.NextMove(state, current_move))
      {
        ne.FirstMove(state, current_move);
        rounds++;
      }
      end = (current_move == start_move) || rounds > 1;
      move_count++;
      computed_move = EvaluatedMove<Move, CostStructure>(current_move);
      return *this;
    }
    EvaluatedMove<Move, CostStructure> operator*() const
    {
      return computed_move;
    }
    EvaluatedMove<Move, CostStructure> *operator->() const
    {
      return &computed_move;
    }
    bool operator==(const StartingNeighborhoodIterator<Input, Solution, Move, CostStructure> &it2) const
    {
      if (end && it2.end)
        return true;
      return (end == it2.end && move_count == it2.move_count && &state == &it2.Solution&& start_move == it2.start_move);
    }
    bool operator!=(const StartingNeighborhoodIterator<Input, Solution, Move, CostStructure> &it2)
    {
      if (end && it2.end)
        return false;
      return (end != it2.end || move_count != it2.move_count || &state != &it2.state || start_move != it2.start_move);
    }
    
  protected:
    StartingNeighborhoodIterator(const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne, const Move& start_move, const Solution &state, bool end = false)
    : ne(ne), state(state), start_move(start_move), rounds(0), move_count(0), end(end)
    {
      if (end)
        return;
      try
      {
        ne.FirstMove(state, current_move);
        current_move = start_move;
        computed_move = EvaluatedMove<Move, CostStructure>(current_move);
      }
      catch (EmptyNeighborhood)
      {
        end = true;
      }
    }
    const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne;
    const Solution &state;
    const Move& start_move;
    unsigned int rounds;
    Move current_move;
    EvaluatedMove<Move, CostStructure> computed_move;
    size_t move_count;
    bool end;
  };

template <class Input, class Solution, class Move, class CostStructure>
class SampleNeighborhoodIterator : public std::iterator<std::input_iterator_tag, EvaluatedMove<Move, CostStructure>>
{
  friend class NeighborhoodExplorerIteratorInterface<Input, Solution, Move, CostStructure>;

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
      computed_move = EvaluatedMove<Move, CostStructure>(current_move, 0);
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
      ne.RandomMove(state, current_move);
      computed_move = EvaluatedMove<Move, CostStructure>(current_move);
    }
    return *this;
  }
  EvaluatedMove<Move, CostStructure> operator*() const
  {
    return computed_move;
  }
  const EvaluatedMove<Move, CostStructure> *operator->() const
  {
    return &computed_move;
  }
  bool operator==(const SampleNeighborhoodIterator<Input, Solution, Move, CostStructure> &it2) const
  {
    if (end && it2.end)
      return true;
    return (end == it2.end && move_count == it2.move_count && &state == &it2.state);
  }
  bool operator!=(const SampleNeighborhoodIterator<Input, Solution, Move, CostStructure> &it2)
  {
    if (end && it2.end)
      return false;
    return (end != it2.end || move_count != it2.move_count || &state != &it2.state);
  }

protected:
  SampleNeighborhoodIterator(const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne, const Solution &state, size_t samples, bool end = false)
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
    computed_move = EvaluatedMove<Move, CostStructure>(current_move);
  }
  const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne;
  const Solution &state;
  Move current_move;
  EvaluatedMove<Move, CostStructure> computed_move;
  size_t move_count, samples;
  bool end;
};

template <class Input, class Solution, class Move, class CostStructure>
class NeighborhoodExplorerIteratorInterface
{
protected:
  static FullNeighborhoodIterator<Input, Solution, Move, CostStructure> create_full_neighborhood_iterator(const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne, const Solution &st, bool end = false)
  {
    return FullNeighborhoodIterator<Input, Solution, Move, CostStructure>(ne, st, end);
  }
  
  static StartingNeighborhoodIterator<Input, Solution, Move, CostStructure> create_starting_neighborhood_iterator(const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne, const Move& start_move, const Solution &st, bool end = false)
  {
    return StartingNeighborhoodIterator<Input, Solution, Move, CostStructure>(ne, start_move, st, end);
  }

  static SampleNeighborhoodIterator<Input, Solution, Move, CostStructure> create_sample_neighborhood_iterator(const NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne, const Solution &st, size_t samples, bool end = false)
  {
    return SampleNeighborhoodIterator<Input, Solution, Move, CostStructure>(ne, st, samples, end);
  }
};

template <class Input, class Solution, class NE>
class ParallelNeighborhoodExplorer : public NE, public NeighborhoodExplorerIteratorInterface<Input, Solution, typename NE::MoveType, typename NE::CostStructureType>
{
public:
  using NE::NE;
  using typename NE::CFtype;
  using typename NE::CostStructureType;
  using typename NE::MoveAcceptor;
  using typename NE::MoveType;

protected:
  FullNeighborhoodIterator<Input, Solution, MoveType, CostStructureType> begin(const Solution &st) const
  {
    return NeighborhoodExplorerIteratorInterface<Input, Solution, MoveType, CostStructureType>::create_full_neighborhood_iterator(*this, st);
  }

  FullNeighborhoodIterator<Input, Solution, MoveType, CostStructureType> end(const Solution &st) const
  {
    return NeighborhoodExplorerIteratorInterface<Input, Solution, MoveType, CostStructureType>::create_full_neighborhood_iterator(*this, st, true);
  }
  
  StartingNeighborhoodIterator<Input, Solution, MoveType, CostStructureType> begin(const MoveType& start_move, const Solution &st) const
  {
    return NeighborhoodExplorerIteratorInterface<Input, Solution, MoveType, CostStructureType>::create_starting_neighborhood_iterator(*this, start_move, st);
  }
  
  StartingNeighborhoodIterator<Input, Solution, MoveType, CostStructureType> end(const MoveType& start_move, const Solution &st) const
  {
    return NeighborhoodExplorerIteratorInterface<Input, Solution, MoveType, CostStructureType>::create_starting_neighborhood_iterator(*this, start_move, st, true);
  }

  SampleNeighborhoodIterator<Input, Solution, MoveType, CostStructureType> sample_begin(const Solution &st, size_t samples) const
  {
    if (samples > 0)
      return NeighborhoodExplorerIteratorInterface<Input, Solution, MoveType, CostStructureType>::create_sample_neighborhood_iterator(*this, st, samples);
    else
      return sample_end(st, samples);
  }

  SampleNeighborhoodIterator<Input, Solution, MoveType, CostStructureType> sample_end(const Solution &st, size_t samples) const
  {
    return NeighborhoodExplorerIteratorInterface<Input, Solution, MoveType, CostStructureType>::create_sample_neighborhood_iterator(*this, st, samples, true);
  }

public:
  virtual EvaluatedMove<MoveType, CostStructureType> SelectFirst(const Solution &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const throw(EmptyNeighborhood)
  {
    EvaluatedMove<MoveType, CostStructureType> first_move;
    bool first_move_found = false;
    tbb::spin_mutex mx_first_move;
    explored = 0;
    tbb::parallel_for_each(this->begin(st), this->end(st), [this, &st, &mx_first_move, &first_move, &first_move_found, AcceptMove, &weights, &explored](EvaluatedMove<MoveType, CostStructureType> &mv) {
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
      return EvaluatedMove<MoveType, CostStructureType>::empty;
    return first_move;
  }
  
  virtual EvaluatedMove<MoveType, CostStructureType> SelectFirst(const MoveType& start_move, const Solution &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const throw(EmptyNeighborhood)
  {
    EvaluatedMove<MoveType, CostStructureType> first_move;
    bool first_move_found = false;
    tbb::spin_mutex mx_first_move;
    explored = 0;
    tbb::parallel_for_each(this->begin(start_move, st), this->end(start_move, st), [this, &st, &mx_first_move, &first_move, &first_move_found, AcceptMove, &weights, &explored](EvaluatedMove<MoveType, CostStructureType> &mv) {
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
      return EvaluatedMove<MoveType, CostStructureType>::empty;
    return first_move;
  }

  virtual EvaluatedMove<MoveType, CostStructureType> SelectBest(const Solution &st, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const throw(EmptyNeighborhood)
  {
    tbb::spin_mutex mx_best_move;
    EvaluatedMove<MoveType, CostStructureType> best_move;
    unsigned int number_of_bests = 0;
    explored = 0;
    tbb::parallel_for_each(this->begin(st), this->end(st), [this, &st, &mx_best_move, &best_move, &number_of_bests, AcceptMove, &weights, &explored](EvaluatedMove<MoveType, CostStructureType> &mv) {
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
          if (Random::Uniform<unsigned int>(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
            best_move = mv;
          number_of_bests++;
        }
      }
    });
    if (number_of_bests == 0)
      return EvaluatedMove<MoveType, CostStructureType>::empty;
    return best_move;
  }

  virtual EvaluatedMove<MoveType, CostStructureType> RandomFirst(const Solution &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const throw(EmptyNeighborhood)
  {
    EvaluatedMove<MoveType, CostStructureType> first_move;
    bool first_move_found = false;
    tbb::spin_mutex mx_first_move;
    explored = 0;
    tbb::parallel_for_each(this->sample_begin(st, samples), this->sample_end(st, samples), [this, &st, &mx_first_move, &first_move, &first_move_found, &explored, AcceptMove, &weights](EvaluatedMove<MoveType, CostStructureType> &mv) {
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
      return EvaluatedMove<MoveType, CostStructureType>::empty;
    return first_move;
  }

  virtual EvaluatedMove<MoveType, CostStructureType> RandomBest(const Solution &st, size_t samples, size_t &explored, const MoveAcceptor &AcceptMove, const std::vector<double> &weights = std::vector<double>(0)) const throw(EmptyNeighborhood)
  {
    tbb::spin_mutex mx_best_move;
    EvaluatedMove<MoveType, CostStructureType> best_move;
    unsigned int number_of_bests = 0;
    explored = 0;
    tbb::parallel_for_each(this->sample_begin(st, samples), this->sample_end(st, samples), [this, &st, &mx_best_move, &best_move, &number_of_bests, &explored, AcceptMove, &weights](EvaluatedMove<MoveType, CostStructureType> &mv) {
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
          if (Random::Uniform<unsigned int>(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
            best_move = mv;
          number_of_bests++;
        }
      }
    });
    if (number_of_bests == 0)
      return EvaluatedMove<MoveType, CostStructureType>::empty;
    return best_move;
  }
};
} // namespace Core
} // namespace EasyLocal

#endif
