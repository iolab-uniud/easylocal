#if defined(TBB_AVAILABLE)

#pragma once

#include "helpers/kicker.hh"
#include <iterator>
#include <tbb/tbb.h>

namespace EasyLocal
{
  namespace Core
  {
    
    template <class Kicker>
    class ParallelKicker : public Kicker
    {
      using Kicker::Kicker;
    public:
      using typename Kicker::Input;
      using typename Kicker::Move;
      using typename Kicker::State;
      using typename Kicker::CFtype;
      using typename Kicker::CostStructure;      
      
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectFirst(size_t length, const Input& in, const State &st) const
      {
        tbb::spin_mutex mx_first_kick;
        Kick<State, Move, CostStructure> first_kick;
        CostStructure first_kick_cost;
        bool first_kick_found = false;
        tbb::parallel_for_each(this->begin(length, in, st), this->end(length, in, st), [this, &in, &mx_first_kick, &first_kick, &first_kick_cost, &first_kick_found](Kick<State, Move, CostStructure> &k) {
          CostStructure cost(0, 0, 0, std::vector<CFtype>(this->sm.CostComponents(), 0));
          for (int i = 0; i < k.size(); i++)
          {
            if (!k[i].first.is_valid)
            {
              k[i].first.cost = this->ne.DeltaCostFunctionComponents(in, k[i].second, k[i].first.move);
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
          return std::make_pair(Kick<State, Move, CostStructure>::empty, CostStructure(std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::vector<CFtype>(this->sm.CostComponents(), std::numeric_limits<CFtype>::infinity())));
        return std::make_pair(first_kick, first_kick_cost);
      }
      
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectBest(size_t length, const Input& in, const State &st) const
      {
        tbb::spin_mutex mx_best_kick;
        Kick<State, Move, CostStructure> best_kick;
        CostStructure best_cost;
        unsigned int number_of_bests = 0;
        tbb::parallel_for_each(this->begin(length, in, st), this->end(length, in, st), [this, &in, &mx_best_kick, &best_kick, &best_cost, &number_of_bests](Kick<State, Move, CostStructure> &k) {
          CostStructure cost(0, 0, 0, std::vector<CFtype>(this->sm.CostComponents(), 0));
          for (int i = 0; i < k.size(); i++)
          {
            if (!k[i].first.is_valid)
            {
              k[i].first.cost = this->ne.DeltaCostFunctionComponents(in, k[i].second, k[i].first.move);
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
            if (Random::Uniform<int>(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_kick = k;
            number_of_bests++;
          }
        });
        return std::make_pair(best_kick, best_cost);
      }
      
      virtual std::pair<Kick<State, Move, CostStructure>, CostStructure> SelectRandom(size_t length, const Input& in, const State &st) const
      {
        Kick<State, Move, CostStructure> k = *this->sample_begin(length, in, st, 1);
        CostStructure zero(0, 0, 0, std::vector<CFtype>(this->sm.CostComponents(), 0));
        CostStructure cost = tbb::parallel_reduce(tbb::blocked_range<typename Kick<State, Move, CostStructure>::iterator>(k.begin(), k.end()), zero,
                                                      [this, &in](const tbb::blocked_range<typename Kick<State, Move, CostStructure>::iterator> &r, CostStructure init) -> CostStructure {
                                                        for (typename Kick<State, Move, CostStructure>::iterator it = r.begin(); it != r.end(); ++it)
                                                        {
                                                          it->first.cost = this->ne.DeltaCostFunctionComponents(in, it->second, it->first.move);
                                                          it->first.is_valid = true;
                                                          init += it->first.cost;
                                                        }
                                                        return init;
                                                      },
                                                      [](const CostStructure &a, const CostStructure &b) -> CostStructure {
                                                        return CostStructure(a + b);
                                                      });
        
        return std::make_pair(k, cost);
      }
    };
  } // namespace Core
} // namespace EasyLocal

#endif
