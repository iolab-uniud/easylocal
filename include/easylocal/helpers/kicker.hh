#if !defined(_KICKER_HH_)
#define _KICKER_HH_

#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** A kicker is a special kind of neighborhood explorer, which can generate sequences of moves of arbitrary length. It is used to provide diversification or intensification strategies.
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class Kicker
    {
    public:
      
      /** Constructor.
       @param ne the @ref NeighborhoodExplorer used to generate the @ref Move
       */
      Kicker(const NeighborhoodExplorer<Input, State, Move, CFtype>& ne) : ne(ne) {}
      
      /** The modality of the @ref Move (warning: not the length of the @ref Move sequences) */
      virtual unsigned int Modality() const
      {
        return ne.Modality();
      }
      
      /** Virtual destructor. */
      virtual ~Kicker() {}
      
      /** Generates the first sequence of @ref Move.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick to apply can be found
       */
      virtual void FirstKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw(EmptyNeighborhood)
      {
        kick.clear();
        kick.resize(length);
        std::vector<State> t_state(length);
        
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
              ne.FirstMove(t_state[cur], kick[cur]);
              while(cur > 0 && !IsRelated(kick[cur-1], kick[cur]))
              {
                if (!ne.NextMove(t_state[cur], kick[cur]))
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
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              if (!ne.NextMove(t_state[cur], kick[cur]))
              {
                backtracking = true;
                cur--;
                goto loop;
              }
            }
            while(cur > 0 && !IsRelated(kick[cur-1], kick[cur]));
            backtracking = false;
            ne.MakeMove(t_state[cur], kick[cur]);
            cur++;
            goto loop;
          }
        }
      }
      
      /** Generates the next sequence of @ref Move.
       @param st current @ref State
       @param kick a previously generated list of @ref Move
       @param length length of the kick
       */
      virtual bool NextKick(const State& st, std::vector<Move>& kick) const
      {
        unsigned int length = kick.size();
        std::vector<State> t_state(length);
        
        // compute states, advance cur
        int cur = 0;
        while (cur < (int)length - 1)
        {
          // reset state before generating each move
          t_state[cur] = cur > 0 ? t_state[cur-1] : st;
          
          // "propagate" moves
          ne.MakeMove(t_state[cur], kick[cur]);
          cur++;
        }
        
        // go to last move, then start generating with backtracking
        bool backtracking = true;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          
          if (cur == -1)
            return false;
          
          // reset state before generating each move
          t_state[cur] = cur > 0 ? t_state[cur-1] : st;
          
          if (!backtracking)
          {
            try
            {
              ne.FirstMove(t_state[cur], kick[cur]);
              while(cur > 0 && !IsRelated(kick[cur-1], kick[cur]))
              {
                if (!ne.NextMove(t_state[cur], kick[cur]))
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
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              if (!ne.NextMove(t_state[cur], kick[cur]))
              {
                backtracking = true;
                cur--;
                goto loop;
              }
            }
            while(cur > 0 && !IsRelated(kick[cur-1], kick[cur]));
            backtracking = false;
            ne.MakeMove(t_state[cur], kick[cur]);
            cur++;
            goto loop;
          }
        }
        
        return true;
      }
      
      /** Generates a random sequence of @ref Move.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick to apply can be found
       */
      virtual void RandomKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw(EmptyNeighborhood)
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
              
              while(cur > 0 && !IsRelated(kick[cur-1], kick[cur]))
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
            while(cur > 0 && !IsRelated(kick[cur-1], kick[cur]));
            backtracking = false;
            ne.MakeMove(t_state[cur], kick[cur]);
            cur++;
            goto loop;
          }
        }
        
      }
      
      /** Computes the cost variation induced by a kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to evaluate
       @return the cost of applying the sequence of @ref Move to the @ref State
       */
      virtual CFtype DeltaCostFunction(const State& st, const std::vector<Move>& kick) const
      {
        CFtype total_delta = 0;
        State t(st);
        
        for (const Move& m : kick)
        {
          total_delta += ne.DeltaCostFunction(t, m);
          ne.MakeMove(t, m);
        }
        
        return total_delta;
      }
      
      /** Generates the first improving kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick can be found
       @return the cost of applying the kick to the @ref State
       */
      virtual CFtype FirstImprovingKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw (EmptyNeighborhood)
      {
        // generate first kick
        FirstKick(st, kick, length);
        CFtype delta = DeltaCostFunction(st, kick);
        
        // continue generating kicks until one with negative delta is found
        while (GreaterThan(delta, 0) && NextKick(st, kick))
          delta = DeltaCostFunction(st, kick);
          
          return delta;
      }
      
      /** Generates the best kick.
       @param st current @ref State
       @param kick the sequence of @ref Move to generate
       @param length length of the kick
       @throws @ref EmptyNeighborhood if no kick can be found
       @return the cost of applying the kick to the @ref State
       */
      virtual CFtype BestKick(const State& st, std::vector<Move>& best_kick, unsigned int length) const throw (EmptyNeighborhood)
      {
        // generate first (best) kick
        FirstKick(st, best_kick, length);
        CFtype best_delta = DeltaCostFunction(st, best_kick);
        
        // candidate kick
        std::vector<Move> kick(best_kick);
        CFtype delta = best_delta;
        
        // keep searching until no kicks can be generated anymore
        while (NextKick(st, kick))
        {
          delta = DeltaCostFunction(st, kick);
          if (GreaterThan(best_delta, delta))
          {
            best_kick = kick;
            best_delta = delta;
          }
        }
        
        return best_delta;
      }
      
      /** Executes a kick on a state.
       @param st the @ref State to modify
       @param kick the sequence of @ref Move to apply
       */
      virtual void MakeKick(State &st, const std::vector<Move>& kick) const
      {
        unsigned int cur = 0, length = kick.size();
        std::vector<State> t_state(length);
        
        while (cur < length)
        {
          // reset state before generating each move
          if (cur > 0)
            t_state[cur] = t_state[cur-1];
          else
            t_state[cur] = st;
          
          // "propagate" moves
          ne.MakeMove(t_state[cur], kick[cur]);
          cur++;
        }
        cur--;
        st = t_state[cur];
      }
      
      
    protected:
      
      /** The @ref NeighborhoodExplorer used */
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      
    };
  }
}

#endif // _KICKER_HH_
