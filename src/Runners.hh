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

/**
 @defgroup Runners Runners
 
 @brief Runner classes are the algorithmic core of the framework. They are 
 responsible for performing a run of a local search technique, 
 starting from an initial state and leading to a final one.  
 
 EasyLocalpp comprises a hierarchy of runners. The base class
 Runner has only @ref Input and @ref State templates,
 and is connected to the solvers, which have no knowledge about the
 neighborhood relations.
 
 The class MoveRunner requires also the template
 @ref Move, and the pointers to the necessary helpers.  It also
 stores the basic data common to all derived classes: the current
 state, the current move, and the number of iterations.
 
 The use of templates allows us to directly define objects of type
 @ref State, such as @c current_state and @c best_state, 
 rather than accessing them through pointers.
 This feature makes the construction and the copy of objects of type
 @ref State completely transparent to the user, since it does not
 require any explicit cast operation or dynamic allocation. 

 */

#if !defined(_RUNNERS_HH_)
#define _RUNNERS_HH_

#include <climits>

#include <runners/SteepestDescent.hh>
#include <runners/FirstDescent.hh>
#include <runners/HillClimbing.hh>
#include <runners/SimulatedAnnealing.hh>
#include <runners/GreatDeluge.hh>
#include <runners/TabuSearch.hh>
// #include <runners/TabuSearchWithShiftingPenalty.hh>

#endif // define _RUNNERS_HH_
