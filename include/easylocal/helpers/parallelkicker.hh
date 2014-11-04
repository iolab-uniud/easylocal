//
//  parallelkicker.hh
//  EasyLocal
//
//  Created by Luca Di Gaspero on 29/10/14.
//
//

#if defined(TBB_AVAILABLE)

#ifndef _parallelkicker_hh
#define _parallelkicker_hh

#include "easylocal/helpers/kicker.hh"
#include <iterator>
#include <tbb/tbb.h>

namespace EasyLocal {
  namespace Core {
    
    template <class State, class Move, typename CFtype>
    struct Kick : public std::vector<std::pair<EvaluatedMove<Move, CFtype>, State>>
    {
    public:
      static Kick empty;
    };
    
    template <class State, class Move, typename CFtype>
    Kick<State, Move, CFtype> Kick<State, Move, CFtype>::empty;
    
    template <class State, class Move, typename CFtype>
    std::ostream& operator<<(std::ostream& os, const Kick<State, Move, CFtype>& k)
    {
      os << "{";
      for (size_t i = 0; i < k.size(); i++)
      {
        if (i > 0)
          os << ", ";
        os << k[i].first.move;
      }
      os << "}";
      return os;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    class ParallelKicker;
    
    template <class Input, class State, class Move, typename CFtype>
    class FullKickerIterator : public std::iterator<std::input_iterator_tag, Kick<State, Move, CFtype>>
    {
      friend class ParallelKicker<Input, State, Move, CFtype>;
    public:
      const FullKickerIterator& operator++(int) // postfix
      {
        if (end)
          throw std::logic_error("Attempting to go after last kick");
        end = !NextKick();
        kick_count++;
        return *this;
      }
      FullKickerIterator operator++() // prefix
      {
        FullKickerIterator ni = *this;
        if (end)
          throw std::logic_error("Attempting to go after last kick");
        end = !NextKick();
        kick_count++;
        return ni;
      }
      Kick<State, Move, CFtype> operator*() const
      {
        return kick;
      }
      Kick<State, Move, CFtype>* operator->() const
      {
        return &kick;
      }
      bool operator==(const FullKickerIterator<Input, State, Move, CFtype>& it2) const
      {
        if (end && it2.end)
          return true;
        return (end == it2.end && length == it2.length && kick_count == it2.kick_count && &start_state == &it2.start_state);
      }
      bool operator!=(const FullKickerIterator<Input, State, Move, CFtype>& it2)
      {
        if (end && it2.end)
          return false;
        return (end != it2.end || length != it2.length || kick_count != it2.kick_count || &start_state != &it2.start_state);
      }
    protected:
      void FirstKick() throw (EmptyNeighborhood)
      {
        kick.clear();
        kick.resize(length, std::make_pair(EvaluatedMove<Move, CFtype>(false), start_state));
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
          
          // reset state before generating each move
          kick[cur].second = cur > 0 ? kick[cur - 1].second : start_state;
          
          if (!backtracking)
          {
            try
            {
              nhe.FirstMove(kick[cur].second, kick[cur].first.move);
              while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move))
              {
                if (!nhe.NextMove(kick[cur].second, kick[cur].first.move))
                {
                  backtracking = true;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              nhe.MakeMove(kick[cur].second, kick[cur].first.move);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              if (!nhe.NextMove(kick[cur].second, kick[cur].first.move))
              {
                backtracking = true;
                cur--;
                goto loop;
              }
            }
            while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move));
            backtracking = false;
            nhe.MakeMove(kick[cur].second, kick[cur].first.move);
            cur++;
            goto loop;
          }
        }
      }
      
      // FIXME: the "evaluated" state of the evaluated move must be reset upon backtracking
      bool NextKick()
      {
        // go to last move, then start generating with backtracking
        int cur = length - 1;
        bool backtracking = true;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {          
          if (cur == -1)
            return false;
          
          // reset state before generating each move
          kick[cur].second = cur > 0 ? kick[cur - 1].second : start_state;
          
          if (!backtracking)
          {
            try
            {
              nhe.FirstMove(kick[cur].second, kick[cur].first.move);
              while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move))
              {
                if (!nhe.NextMove(kick[cur].second, kick[cur].first.move))
                {
                  backtracking = true;
                  kick[cur].first.is_valid = false;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              nhe.MakeMove(kick[cur].second, kick[cur].first.move);
              kick[cur].first.is_valid = false;
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              if (!nhe.NextMove(kick[cur].second, kick[cur].first.move))
              {
                backtracking = true;
                kick[cur].first.is_valid = false;
                cur--;
                goto loop;
              }
            }
            while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move));
            backtracking = false;
            nhe.MakeMove(kick[cur].second, kick[cur].first.move);
            kick[cur].first.is_valid = false;
            cur++;
            goto loop;
          }
        }
        return true;
      }
      //protected:
    public:
      FullKickerIterator<Input, State, Move, CFtype>(size_t length, const NeighborhoodExplorer<Input, State, Move, CFtype>& nhe, const State& state, bool end = false)
      : length(length), nhe(nhe), start_state(state), kick_count(0), end(end)
      {
        if (end)
          return;
        try
        {
          FirstKick();
        }
        catch (EmptyNeighborhood)
        {
          end = true;
        }
      }
      const size_t length;
      const NeighborhoodExplorer<Input, State, Move, CFtype>& nhe;
      const State& start_state;
      Kick<State, Move, CFtype> kick;
      size_t kick_count;
      bool end;
    };
    
    template <class Input, class State, class Move, typename CFtype = int>
    class ParallelKicker
    {
    protected:
      FullKickerIterator<Input, State, Move, CFtype> begin(size_t length, const State& st) const
      {
        return FullKickerIterator<Input, State, Move, CFtype>(length, nhe, st);
      }
      
      FullKickerIterator<Input, State, Move, CFtype> end(size_t length, const State& st) const
      {
        return FullKickerIterator<Input, State, Move, CFtype>(length, nhe, st, true);
      }
      
      const NeighborhoodExplorer<Input, State, Move, CFtype>& nhe;
    public:
      ParallelKicker(const NeighborhoodExplorer<Input, State, Move, CFtype>& nhe) : nhe(nhe) {}
      
      virtual std::pair<Kick<State, Move, CFtype>, CostStructure<CFtype>> SelectFirst(size_t length, const State &st) const throw (EmptyNeighborhood)
      {
        tbb::spin_mutex mx_first_kick;
        Kick<State, Move, CFtype> first_kick;
        CostStructure<CFtype> first_kick_cost;
        bool first_kick_found = false;
        tbb::parallel_for_each(this->begin(length, st), this->end(length, st), [this, &st, &mx_first_kick, &first_kick, &first_kick_cost, &first_kick_found](Kick<State, Move, CFtype>& k) {
          CostStructure<CFtype> cost(0, 0, 0, std::vector<CFtype>(CostComponent<Input, State, CFtype>::CostComponents(), 0));
          for (int i = 0; i < k.size(); i++)
          {
            if (!k[i].first.is_valid)
            {
              k[i].first.cost = this->nhe.DeltaCostFunctionComponents(k[i].second, k[i].first.move);
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
          return std::make_pair(Kick<State, Move, CFtype>::empty, CostStructure<CFtype>(std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::vector<CFtype>(CostComponent<Input, State, CFtype>::CostComponents(), std::numeric_limits<CFtype>::infinity())));
        return std::make_pair(first_kick, first_kick_cost);
      }

      
      virtual std::pair<Kick<State, Move, CFtype>, CostStructure<CFtype>> SelectBest(size_t length, const State &st) const throw (EmptyNeighborhood)
      {
        tbb::spin_mutex mx_best_kick;
        Kick<State, Move, CFtype> best_kick;
        CostStructure<CFtype> best_cost;
        unsigned int number_of_bests = 0;
        tbb::parallel_for_each(this->begin(length, st), this->end(length, st), [this, &st, &mx_best_kick, &best_kick, &best_cost, &number_of_bests](Kick<State, Move, CFtype>& k) {
          CostStructure<CFtype> cost(0, 0, 0, std::vector<CFtype>(CostComponent<Input, State, CFtype>::CostComponents(), 0));
          for (int i = 0; i < k.size(); i++)
          {
            if (!k[i].first.is_valid)
            {
              k[i].first.cost = this->nhe.DeltaCostFunctionComponents(k[i].second, k[i].first.move);
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
            if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_kick = k;
            number_of_bests++;
          }
        });
        return std::make_pair(best_kick, best_cost);
      }
    };
  }
}

#endif

#endif
