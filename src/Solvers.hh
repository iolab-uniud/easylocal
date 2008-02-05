#ifndef SOLVERS_HH_
#define SOLVERS_HH_

/** @defgroup Solvers Solver classes
    Solver classes control the search by generating the initial solutions, 
    and deciding how, and in which sequence,
    runners have to be activated (e.g. tandem, multistart, hybrid
    search). In addition, they communicate with the external
    environment, by getting the input and delivering the output. They
    are linked to one or more runners (for simple or composite
    search, respectively) and to some of the helpers.  
*/  
#include "solvers/Solver.hh"
#include "solvers/AbstractLocalSearchSolver.hh"
#include "solvers/SimpleLocalSearch.hh"
#include "solvers/GeneralizedLocalSearchSolver.hh"

#endif /*SOLVERS_HH_*/
