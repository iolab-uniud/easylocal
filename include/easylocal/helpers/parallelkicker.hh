#if defined(TBB_AVAILABLE)

#pragma once

#include "easylocal/helpers/kicker.hh"
#include <iterator>
#include <tbb/tbb.h>

namespace EasyLocal
{
namespace Core
{

template <class Input, class State, class K>
class ParallelKicker : public K
{
  using K::K;
  using typename K::CFtype;
  using typename K::CostStructureType;
  using typename K::MoveType;

public:
  virtual std::pair<Kick<State, MoveType, CostStructureType>, CostStructureType> SelectFirst(size_t length, const State &st) const
  {
    tbb::spin_mutex mx_first_kick;
    Kick<State, MoveType, CostStructureType> first_kick;
    CostStructureType first_kick_cost;
    bool first_kick_found = false;
    tbb::parallel_for_each(this->begin(length, st), this->end(length, st), [this, &mx_first_kick, &first_kick, &first_kick_cost, &first_kick_found](Kick<State, MoveType, CostStructureType> &k) {
      CostStructureType cost(0, 0, 0, std::vector<CFtype>(this->sm.CostComponents(), 0));
      for (int i = 0; i < k.size(); i++)
      {
        if (!k[i].first.is_valid)
        {
          k[i].first.cost = this->ne.DeltaCostFunctionComponents(k[i].second, k[i].first.move);
          k[i].first.is_valid = true;
        }
        cost += k[i].first.cost;
      }
      tbb::spin_mutex::scoped_lock lock(mx_first_kick);
      if (!first_kick_found)
      {
        if (cost < 0)
        {
          first_kick_found = true;
          first_kick = k;
          first_kick_cost = cost;
          tbb::task::self().cancel_group_execution();
        }
      }
    });
    if (!first_kick_found)
      return std::make_pair(Kick<State, MoveType, CostStructureType>::empty, CostStructureType(std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::vector<CFtype>(this->sm.CostComponents(), std::numeric_limits<CFtype>::infinity())));
    return std::make_pair(first_kick, first_kick_cost);
  }

  virtual std::pair<Kick<State, MoveType, CostStructureType>, CostStructureType> SelectBest(size_t length, const State &st) const
  {
    tbb::spin_mutex mx_best_kick;
    Kick<State, MoveType, CostStructureType> best_kick;
    CostStructureType best_cost;
    unsigned int number_of_bests = 0;
    tbb::parallel_for_each(this->begin(length, st), this->end(length, st), [this, &mx_best_kick, &best_kick, &best_cost, &number_of_bests](Kick<State, MoveType, CostStructureType> &k) {
      CostStructureType cost(0, 0, 0, std::vector<CFtype>(this->sm.CostComponents(), 0));
      for (int i = 0; i < k.size(); i++)
      {
        if (!k[i].first.is_valid)
        {
          k[i].first.cost = this->ne.DeltaCostFunctionComponents(k[i].second, k[i].first.move);
          k[i].first.is_valid = true;
        }
        cost += k[i].first.cost;
      }
      tbb::spin_mutex::scoped_lock lock(mx_best_kick);
      if (number_of_bests == 0)
      {
        best_kick = k;
        best_cost = cost;
        number_of_bests = 1;
      }
      else if (cost < best_cost)
      {
        best_kick = k;
        best_cost = cost;
        number_of_bests = 1;
      }
      else if (cost == best_cost)
      {
        if (Random::Uniform<unsigned int>(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
          best_kick = k;
        number_of_bests++;
      }
    });
    return std::make_pair(best_kick, best_cost);
  }

  virtual std::pair<Kick<State, MoveType, CostStructureType>, CostStructureType> SelectRandom(size_t length, const State &st) const throw(EmptyNeighborhood)
  {
    Kick<State, MoveType, CostStructureType> k = *this->sample_begin(length, st, 1);
    CostStructureType zero(0, 0, 0, std::vector<CFtype>(this->sm.CostComponents(), 0));
    CostStructureType cost = tbb::parallel_reduce(tbb::blocked_range<typename Kick<State, MoveType, CostStructureType>::iterator>(k.begin(), k.end()), zero,
                                                  [this](const tbb::blocked_range<typename Kick<State, MoveType, CostStructureType>::iterator> &r, CostStructureType init) -> CostStructureType {
                                                    for (typename Kick<State, MoveType, CostStructureType>::iterator it = r.begin(); it != r.end(); ++it)
                                                    {
                                                      it->first.cost = this->ne.DeltaCostFunctionComponents(it->second, it->first.move);
                                                      it->first.is_valid = true;
                                                      init += it->first.cost;
                                                    }
                                                    return init;
                                                  },
                                                  [](const CostStructureType &a, const CostStructureType &b) -> CostStructureType {
                                                    return CostStructureType(a + b);
                                                  });

    return std::make_pair(k, cost);
  }
};
} // namespace Core
} // namespace EasyLocal

#endif
