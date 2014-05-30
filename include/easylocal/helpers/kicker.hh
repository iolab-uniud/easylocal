#if !defined(_KICKER_HH_)
#define _KICKER_HH_

#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/helpers/statemanager.hh"

namespace EasyLocal {

  namespace Core {
    
    template <class Input, class State, class Move, typename CFtype>
    class Kicker 
    {
    public:
      
      Kicker(const Input& in, StateManager<Input, State, CFtype>& sm, const NeighborhoodExplorer<Input, State, Move, CFtype>& ne) : ne(ne), sm(sm) { }
      
      void FirstKick(const State& st, std::vector<Move>& kick, unsigned int length) throw(EmptyNeighborhood) const
      {
        unsigned int i = 0;
        kick.resize(length);
        while (i < length)
        {
          // if there are not moves which must be checked for relatedness
          ne.FirstMove(st, kick[i]);
          if (i > 0)
          {
            while (!IsRelated(kick[i-1], kick[i]))
              ne.FirstMove(st, kick[i]);
            
          }
        }
      }
      
      void NextKick(const State& st, std::vector<Move>& kick, unsigned int length) const
      {
        
      }
      
      void FirstKick(const State& st, std::vector<Move>& kick, unsigned int length) const;
      
      
    protected:
      
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      StateManager<Input, State, CFtype>& sm;
      
    };
    
  }
  
}

#endif // _KICKER_HH_