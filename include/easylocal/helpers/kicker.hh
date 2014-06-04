#if !defined(_KICKER_HH_)
#define _KICKER_HH_

#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {

  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype>
    class Kicker 
    {
    public:
      
      /** Constructor. */
      Kicker(const Input& in, const NeighborhoodExplorer<Input, State, Move, CFtype>& ne) : ne(ne), in(in) { }
      
      virtual unsigned int Modality() const 
      {
        return ne.Modality();
      }
      
      /** Virtual destructor. */
      virtual ~Kicker() { }
      
      
      /** Generates the first sequence of moves. 
          @param st current state
          @param kick the sequence of moves to generate
          @param length length of the kick
          @throws EmptyNeighborhood if no kick to apply can be found
      */
      virtual void FirstKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw(EmptyNeighborhood)
      { 
        kick.clear();
        kick.resize(length);
        std::vector<State> t_state(length);
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an EmptyNeighborhood
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

      /** Generates the next sequence of moves. 
          @param st current state
          @param kick a previously generated list of moves
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
                
        // stop only when a complete kicker has been generated, or throw an EmptyNeighborhood
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
      
      virtual void RandomKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw(EmptyNeighborhood)
      {                        
        kick.clear();
        kick.resize(length);
        
        std::vector<State> t_state(length);
        std::vector<Move> initial_move(length);
        std::vector<bool> initial_set(length, false);
        
        int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an EmptyNeighborhood
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
          else // backtracking (we only need to generate NextMoves)
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
      
      virtual CFtype FirstImprovingKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw (EmptyNeighborhood)
      {
        // generate first kick
        FirstKick(st, kick, length);
        CFtype delta = DeltaCostFunction(st, kick);
        
        while (GreaterThan(delta, 0) && NextKick(st, kick))
        {
          delta = DeltaCostFunction(st, kick);
        }
        
        return delta;
      }
      
      virtual CFtype BestKick(const State& st, std::vector<Move>& best_kick, unsigned int length) const throw (EmptyNeighborhood)
      {
        // generate first (best) kick
        FirstKick(st, best_kick, length);
        CFtype best_delta = DeltaCostFunction(st, best_kick);
        
        // candidate kick
        std::vector<Move> kick(best_kick);
        CFtype delta = best_delta;
        
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
      
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      const Input& in;
      
    };
    
  }
  
}

#endif // _KICKER_HH_