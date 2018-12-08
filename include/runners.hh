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

#pragma once

#include "runners/steepestdescent.hh"
#include "runners/firstdescent.hh"
#include "runners/hillclimbing.hh"
#include "runners/simulatedannealing.hh"
#include "runners/abstractsimulatedannealing.hh"
#include "runners/simulatedannealingevaluationbased.hh"
#include "runners/simulatedannealingwithreheating.hh"
#include "runners/greatdeluge.hh"
#include "runners/tabusearch.hh"
#include "runners/firstimprovementtabusearch.hh"
#include "runners/sampletabusearch.hh"
#include "runners/lateacceptancehillclimbing.hh"

