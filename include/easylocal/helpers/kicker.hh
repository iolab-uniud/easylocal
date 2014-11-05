#if !defined(_KICKER_HH_)
#define _KICKER_HH_

#include "easylocal/helpers/neighborhoodexplorer.hh"

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
    class Kicker;
    
    template <class Input, class State, class Move, typename CFtype>
    class FullKickerIterator : public std::iterator<std::input_iterator_tag, Kick<State, Move, CFtype>>
    {
      friend class Kicker<Input, State, Move, CFtype>;
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
      const Kick<State, Move, CFtype>& operator*() const
      {
        return kick;
      }
      Kick<State, Move, CFtype>& operator*()
      {
        return kick;
      }
      const Kick<State, Move, CFtype>* operator->() const
      {
        return &kick;
      }
      Kick<State, Move, CFtype>* operator->()
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
                ne.FirstMove(kick[cur].second, kick[cur].first.move);
                while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move))
                {
                  if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                  {
                    backtracking = true;
                    cur--;
                    goto loop;
                  }
                }
                backtracking = false;
                ne.MakeMove(kick[cur].second, kick[cur].first.move);
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
                if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                {
                  backtracking = true;
                  cur--;
                  goto loop;
                }
              }
              while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move));
              backtracking = false;
              ne.MakeMove(kick[cur].second, kick[cur].first.move);
              cur++;
              goto loop;
            }
        }
      }
      
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
              ne.FirstMove(kick[cur].second, kick[cur].first.move);
              while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move))
              {
                if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                {
                  backtracking = true;
                  kick[cur].first.is_valid = false;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              ne.MakeMove(kick[cur].second, kick[cur].first.move);
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
              if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              {
                backtracking = true;
                kick[cur].first.is_valid = false;
                cur--;
                goto loop;
              }
            }
            while (cur > 0 && !IsRelated(kick[cur - 1].first.move, kick[cur].first.move));
            backtracking = false;
            ne.MakeMove(kick[cur].second, kick[cur].first.move);
            kick[cur].first.is_valid = false;
            cur++;
            goto loop;
          }
        }
        return true;
      }
    protected:
      FullKickerIterator<Input, State, Move, CFtype>(size_t length, const NeighborhoodExplorer<Input, State, Move, CFtype>& ne, const State& state, bool end = false)
      : length(length), ne(ne), start_state(state), kick_count(0), end(end)
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
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      const State& start_state;
      Kick<State, Move, CFtype> kick;
      size_t kick_count;
      bool end;
    };
    
    /** A kicker is a special kind of neighborhood explorer, which can generate sequences of moves of arbitrary length. It is used to provide diversification or intensification strategies.
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class Kicker
    {
    public:
      
      /** Constructor.
       @param ne the @ref NeighborhoodExplorer used to generate the @ref Move
       */
      Kicker(NeighborhoodExplorer<Input, State, Move, CFtype>& ne) : ne(ne) {}
      
      /** The modality of the @ref Move (warning: not the length of the @ref Move sequences) */
      virtual unsigned int Modality() const
      {
        return ne.Modality();
      }
      
      /** Virtual destructor. */
      virtual ~Kicker() {}
      
      /** Generates a random sequence of @ref Move.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick to apply can be found
       */
      /* virtual void RandomKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw(EmptyNeighborhood)
      {
        kick.clear();
        kick.resize(length);
        
        std::vector<State> t_state(length);
        std::vector<Move> initial_move(length);
        std::vector<bool> initial_set(length, false);
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
            
            // reset state before generating each move
            t_state[cur] = cur > 0 ? t_state[cur-1] : st;
          
          if (!backtracking)
          {
            try
            {
              ne.RandomMove(t_state[cur], kick[cur]);
              
              if (!initial_set[cur])
              {
                initial_move[cur] = kick[cur];
                initial_set[cur] = true;
              }
              
              while (cur > 0 && !IsRelated(kick[cur-1], kick[cur]))
              {
                if (!ne.NextMove(t_state[cur], kick[cur], initial_move[cur]))
                {
                  backtracking = true;
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              ne.MakeMove(t_state[cur], kick[cur]);
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
          else // backtracking (we only need to generate moves following the first)
          {
            do
            {
              if (!ne.NextMove(t_state[cur], kick[cur], initial_move[cur]))
              {
                backtracking = true;
                cur--;
                goto loop;
              }
            }
            while (cur > 0 && !IsRelated(kick[cur-1], kick[cur]));
            backtracking = false;
            ne.MakeMove(t_state[cur], kick[cur]);
            cur++;
            goto loop;
          }
        }
        
      } */
      
      /** Generates the first improving kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick can be found
       @return the cost of applying the kick to the @ref State
       */
      virtual std::pair<Kick<State, Move, CFtype>, CostStructure<CFtype>> SelectFirst(size_t length, const State &st) const throw (EmptyNeighborhood)
      {
        for (FullKickerIterator<Input, State, Move, CFtype> it = begin(length, st); it != end(length, st); it++)
        {
          CostStructure<CFtype> cost(0, 0, 0, std::vector<CFtype>(CostComponent<Input, State, CFtype>::CostComponents(), 0));
          for (int i = 0; i < it->size(); i++)
          {
            if (!(*it)[i].first.is_valid)
            {
              (*it)[i].first.cost = this->ne.DeltaCostFunctionComponents((*it)[i].second, (*it)[i].first.move);
              (*it)[i].first.is_valid = true;
            }
            cost += (*it)[i].first.cost;
          }
          if (cost < 0)
            return std::make_pair(*it, cost);
        }
        return std::make_pair(Kick<State, Move, CFtype>::empty, CostStructure<CFtype>(std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::numeric_limits<CFtype>::infinity(), std::vector<CFtype>(CostComponent<Input, State, CFtype>::CostComponents(), std::numeric_limits<CFtype>::infinity())));
      }
      
      /** Generates the best kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick can be found
       @return the cost of applying the kick to the @ref State
       */
      virtual std::pair<Kick<State, Move, CFtype>, CostStructure<CFtype>> SelectBest(size_t length, const State &st) const throw (EmptyNeighborhood)
      {
        Kick<State, Move, CFtype> best_kick;
        CostStructure<CFtype> best_cost;
        unsigned int number_of_bests = 0;
        for (FullKickerIterator<Input, State, Move, CFtype> it = begin(length, st); it != end(length, st); it++)
        {
          CostStructure<CFtype> cost(0, 0, 0, std::vector<CFtype>(CostComponent<Input, State, CFtype>::CostComponents(), 0));
          for (int i = 0; i < it->size(); i++)
          {
            if (!(*it)[i].first.is_valid)
            {
              (*it)[i].first.cost = ne.DeltaCostFunctionComponents((*it)[i].second, (*it)[i].first.move);
              (*it)[i].first.is_valid = true;
            }
            cost += (*it)[i].first.cost;
          }
          if (number_of_bests == 0)
          {
            best_kick = *it;
            best_cost = cost;
            number_of_bests = 1;
          }
          else if (cost < best_cost)
          {
            best_kick = *it;
            best_cost = cost;
            number_of_bests = 1;
          }
          else if (cost == best_cost)
          {
            if (Random::Int(0, number_of_bests) == 0) // accept the move with probability 1 / (1 + number_of_bests)
              best_kick = *it;
            number_of_bests++;
          }
        }
        return std::make_pair(best_kick, best_cost);
      }
      
      /** Executes a kick on a state.
       @param st the @ref State to modify
       @param kick the sequence of @ref Move to apply
       */
      virtual void MakeKick(State &st, const Kick<State, Move, CFtype>& kick) const
      {
        st = kick[kick.size() - 1].second;
      }
      
      
      FullKickerIterator<Input, State, Move, CFtype> begin(size_t length, const State& st) const
      {
        return FullKickerIterator<Input, State, Move, CFtype>(length, ne, st);
      }
      
      FullKickerIterator<Input, State, Move, CFtype> end(size_t length, const State& st) const
      {
        return FullKickerIterator<Input, State, Move, CFtype>(length, ne, st, true);
      }
    protected:
      
      /** The @ref NeighborhoodExplorer used */
      NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      
    };
  }
}

#endif // _KICKER_HH_
