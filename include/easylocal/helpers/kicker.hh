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
      
    protected:
      
      const NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
      StateManager<Input, State, CFtype>& sm;
      
    };
    
  }
  
}

#endif // _KICKER_HH_