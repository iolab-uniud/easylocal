#ifndef HELPERS_HH_
#define HELPERS_HH_

/** @defgroup Helpers Helper classes
      Helper classes perform actions related to some specific aspects of the
      search. For example, the @c NeighborhoodExplorer is responsible
      for everything concerning the neighborhood: candidate move
      selection, update current state by executing a move, and so on.
      Different @c NeighborhoodExplorers may be defined in case of
      composite search, each one handling a specific neighborhood
      relation used by the algorithm.  

      Helper classes cooperate among themselves.  For example, the @c
      NeighborhoodExplorer is not responsible for the move prohibition
      mechanisms (such as maintaining the tabu list), which are
      delegated to another helper, namely the @c ProibitionManager.
  
      Helper classes do not have their own internal data, but they work on the
      internal state of the runners that invoke them, and interact with
      them through function parameters.  
  */

#include "helpers/StateManager.hh"
#include "helpers/OutputManager.hh"
#include "helpers/NeighborhoodExplorer.hh"
#include "helpers/TabuListManager.hh"
#include "helpers/ShiftingPenaltyManager.hh"
#include "helpers/CostComponent.hh"
#include "helpers/DeltaCostComponent.hh"
#include "helpers/GeneralizedLocalSearchObserver.hh"
#include "helpers/RunnerObserver.hh"
//#include "helpers/KickerObserver.hh"

#endif /*HELPERS_HH_*/
