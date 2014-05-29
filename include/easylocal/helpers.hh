#if !defined(_HELPERS_HH_)
#define _HELPERS_HH_

/** 
 @defgroup Helpers Helpers
 
 @brief Helpers perform actions related to some specific aspects of the search and have to be
 partially implemented by the user.
 
 EasyLocalpp defines a number of helper classes that are not related
 hierarchically, but they are linked to @ref Runners, @ref Solvers, @ref Testers 
 and to each other through references. The helpers are the following ones:
 
 - StateManager: is responsible for all operations on the state that are
 independent of the neighborhood definition (i.e., from the @ref Move "Moves").
 
 - CostComponent: is responsible for computing a component of the cost function
 on a given @ref State. The class is able to handle both @e hard and @e soft cost components.
 
 - OutputManager:
 is responsible for translating between elements of the search space
 and output solutions.  It also delivers other output information of
 the search, and stores and retrieves solutions from files.  This is
 the only helper that deals with the @ref Output class.  All
 other helpers work only on the @ref State class, which
 represents the elements of the search space used by the algorithms.
 
 - NeighborhoodExplorer: handles all the features concerning neighborhood exploration in an @e iterator
 fashion.
 
 - DeltaCostComponent: is responsible for computing the difference of the cost function due to the application
 of a @ref Move on a given @ref State.
 
 - ProhibitionManager: is in charge for the management of the prohibition
 mechanism (e.g., the TabuListManager for the short-term memory tabu search strategy).
 
 - ShiftingPenaltyManager: is responsible for the adaptive modification of the weights of the
 cost function, according to the @e shifting @e penalty mechanism. For each component of the
 cost function, it maintains an independent weight, which varies depending on the number of 
 violations and according to a customizable scheme.
*/

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/outputmanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/helpers/tabulistmanager.hh"
#include "easylocal/helpers/costcomponent.hh"
#include "easylocal/helpers/deltacostcomponent.hh"
#include "easylocal/helpers/prohibitionmanager.hh"
#include "easylocal/helpers/multimodalneighborhoodexplorer.hh"
#include "easylocal/helpers/multimodaltabulistmanager.hh"

// #include "helpers/ShiftingPenaltyManager.hh"

#endif // _HELPERS_HH_ 
