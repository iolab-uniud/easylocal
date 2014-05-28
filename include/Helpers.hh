// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

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

#include "helpers/StateManager.hh"
#include "helpers/OutputManager.hh"
#include "helpers/NeighborhoodExplorer.hh"
#include "helpers/TabuListManager.hh"
#include "helpers/CostComponent.hh"
#include "helpers/DeltaCostComponent.hh"
// #include "helpers/ShiftingPenaltyManager.hh"


#endif // _HELPERS_HH_ 
