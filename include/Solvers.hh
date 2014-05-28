#if !defined(_SOLVERS_HH_)
#define _SOLVERS_HH_

/** @defgroup Solvers Solvers
 
@brief Solver classes control the search by generating the initial solutions, 
and deciding how, and in which sequence, Runners and Kickers have to be activated.
*/  

#include "solvers/SimpleLocalSearch.hh"
#include "solvers/VariableNeighborhoodDescent.hh"
#include "solvers/TokenRingSearch.hh"
//#include "solvers/GeneralizedLocalSearch.hh"
//#include "solvers/GRASP.hh"

#endif // _SOLVERS_HH_
