#if !defined(_KICKER_HH_)
#define _KICKER_HH_

#include "easylocal/helpers/neighborhoodexplorer.hh"

namespace EasyLocal {

  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype>
    class Kicker 
    {
    public:
      
      Kicker(const Input& in, const NeighborhoodExplorer<Input, State, Move, CFtype>& ne) : ne(ne) { }
      
      /** Generates the first sequence of moves. 
          @param st current state
          @param kick the sequence of moves to generate
          @param length length of the kick
          @throws EmptyNeighborhood if no kick to apply can be found
      */
      void FirstKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw(EmptyNeighborhood)
      { 
        kick.clear();
        kick.resize(length);
        std::vector<State> t_state(length);
        
        unsigned int cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an EmptyNeighborhood
        while (cur < length)
        {
          // reset state before generating each move
          if (cur > 0)
            t_state[cur] = t_state[cur-1];
          else
            t_state[cur] = st;
          
          // if we're going forward (not backtracking), try first move first, if we're backtracking, skip directly to next move
          if (!backtracking)
          {
            try 
            {
              ne.FirstMove(t_state[cur], kick[cur]);
            }
            catch (EmptyNeighborhood)
            {
              // backtrack if possible, otherwise raise exception
              if (cur > 0)
              {
                backtracking = true;
                cur--;
              }
              else
                throw EmptyNeighborhood();
            }
          }
          else
          {
            // generate next move (fails if all neighborhood has been generated)
            if (!ne.NextMove(t_state[cur], kick[cur]))
            {
              // backtrack if possible, otherwise raise exception
              if (cur > 0)
              {
                backtracking = true;
                cur--;
              }
              else
                throw EmptyNeighborhood();
            }
          }
          
          // keep generating next move until move is related to previous one 
          while (!IsRelated(kick[cur-1],kick[cur]))
          {
            if (!ne.NextMove(t_state[cur], kick[cur]))
            {
              // backtrack if possible, otherwise raise exception
              if (cur > 0)
              {
                backtracking = true;
                cur--;
              }
              else
                throw EmptyNeighborhood();
            }
          }
          
          // "propagate" moves
          ne.MakeMove(t_state[cur], kick[cur]);
          backtracking = false;
          cur++;
        }
      }
      
      /** Generates the next sequence of moves. 
          @param st current state
          @param kick a previously generated list of moves
          @param length length of the kick
      */
      bool NextKick(const State& st, std::vector<Move>& kick, unsigned int length)
      { 
        std::vector<State> t_state(length);
        
        // compute states, advance cur
        unsigned int cur = 0;
        while (cur < length - 1)
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
        
        // go to last move, then start generating with backtracking
        bool backtracking = true;
                
        // stop only when a complete kicker has been generated, or throw an EmptyNeighborhood
        while (cur < length)
        {
          // reset state before generating each move
          if (cur > 0)
            t_state[cur] = t_state[cur-1];
          else
            t_state[cur] = st;
          
          // if we're going forward, try first move first, if we're backtracking, skip directly to next move
          if (!backtracking)
          {
            try 
            {
              ne.FirstMove(t_state[cur], kick[cur]);
            }
            catch(EmptyNeighborhood)
            {
              // backtrack if possible, otherwise raise exception
              if (cur > 0)
              {
                backtracking = true;
                cur--;
              }
              else
                return false;
            }
          }
          else
          {
            // generate next move (fails if all neighborhood has been generated or when an empty neighborhood is tried)
            if (!ne.NextMove(t_state[cur], kick[cur]))
            {
              // backtrack if possible, otherwise raise exception
              if (cur > 0)
              {
                backtracking = true;
                cur--;
              }
              else
                return false;
            }
          }
          
          // keep generating next move until move is related to previous one 
          while (!IsRelated(kick[cur-1],kick[cur]))
          {
            // generate next move (fails if all neighborhood has been generated or when an empty neighborhood is tried)
            if (!ne.NextMove(t_state[cur], kick[cur]))
            {
              // backtrack if possible, otherwise raise exception
              if (cur > 0)
              {
                backtracking = true;
                cur--;
              }
              else
                return false;
            }
          }
          
          // "propagate" moves
          ne.MakeMove(t_state[cur], kick[cur]);
          backtracking = false;
          cur++;
        }
        
        return true;
      }
      
      void RandomKick(const State& st, std::vector<Move>& kick, unsigned int length) const throw(EmptyNeighborhood)
      {
        throw EmptyNeighborhood();
      }
      
      
    protected:
      
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      
    };
    
  }
  
}

#endif // _KICKER_HH_