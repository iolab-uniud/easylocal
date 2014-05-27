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
 @file EasyLocal.hh
 @brief Full class inclusions.
  
 This file contains the inclusions of @e all the files for 
 the class declarations of the EasyLocal framework.   
 
 @author Luca Di Gaspero, Andrea Schaerf - University of Udine, Italy
 @version 1.0
 $Revision$
 $Date$
 @note This version works both with MS Visual C++ and the GNU C++ 
 compiler. Yet, it is extensively tested only with the GNU compiler.
 
 */

#if !defined(_EASYLOCAL_HH_)
#define _EASYLOCAL_HH_

#include <Utils.hh>
#include <Helpers.hh>
#include <Runners.hh>
#include <Kickers.hh>
#include <Solvers.hh>

#endif // _EASYLOCAL_HH_

/**
 @mainpage EasyLocalpp: an Object-Oriented framework for Local Search algorithms.
 
 This is EasyLocalpp: a C++ Object-Oriented framework
 aimed at easing the development of Local Search algorithms.
 
 Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
 
 The abstract classes that compose the framework specify and implement
 the invariant part of the algorithm, and are meant to be specialized by 
 concrete classes that supply the problem-dependent part. 
 The framework provides the full control structures of the algorithms, 
 and the user has only to write the problem-specific code. Furthermore, 
 the framework comes out with some tools that simplify the analysis of 
 the algorithms.  
 
 The architecture of EasyLocalpp provides a principled modularization for
 the solution of combinatorial problems by local search, and helps the
 user by deriving a neat conceptual scheme of the application.  It also
 supports the design of combinations of basic techniques and/or
 neighborhood structures.
 
 The core of EasyLocalpp is composed of a set of cooperating classes that
 take care of different aspects of local search. The user's application
 is obtained by writing derived classes for a selected subset of the
 framework ones. Such user-defined classes contain only the specific
 problem description, but no control information for the algorithm. In
 fact, the relationships between classes, and their interactions by
 mutual method invocation, are completely dealt with by the framework.
 
 The classes in the framework are split in five categories, depending
 on the role they play in a local search algorithm. We have identified
 the following sets of classes:
 
 - @ref Data "Data" classes: store the basic data of the algorithm.  They
 encode the @ref State "State"s of the search space, the @ref Move "Move"s, 
 and the @ref Input "Input"/@ref Output "Output" data.  
 These classes have only data members and no
 methods, except for those accessing their own data.  They have no
 computing capabilities and, generally, no links to other
 classes.
 
 - @ref Helpers "Helpers" perform actions related to some specific aspects of the
 search, such as the generation of an initial random state or the
 exploration of the neighborhood of a given state. 
 Helpers do not have their own internal data, but they work on the
 internal state of the @ref Runners "Runners" that invoke them, and interact with
 them through function parameters.
 
 - @ref Runners "Runners" are the algorithmic core of the framework. They are
 responsible for performing a run of a local search technique,
 starting from an initial state and leading to a final one. Each
 runner has many data objects that represent the state of the
 search (current state, best state, current move, number of
 iterations, ...), and it maintains links to all the helpers, which
 are invoked for performing specific tasks on its own data. Example
 of runners are @e tabu @e search and @e simulated @e annealing.
 
 - @ref Kickers "Kickers" implement intensification and/or diversification
 strategies based on macro-moves in larger neighborhoods composed
 by sequences of basic moves.
 
 - @ref Solvers "Solvers" control the search by generating the initial solutions,
 and deciding how, and in which sequence, runners have to be
 activated. In addition, they communicate with the external environment, 
 by getting the input and delivering the output. They are linked to one or more runners
 (for simple or composite search, respectively) and to some of the
 helpers.
 
 Other components of EasyLocalpp are:
 
 - @ref Observers "Observers" are a set of classes for inspecting the behavior of
 the local search algorithm. They are listeners to the local search events and 
 they output the progress of the local search to an output stream.
 
 - @ref Testers "Testers" represent a simple predefined interface of the user
 program. They can be used to help the developers in debugging their
 code, adjusting the techniques, and tuning the parameters.
 The testers are not used anymore whenever the program is embedded in
 a larger application, or if the users develop an @e ad @e hoc
 interface for their programs. 
 
 - @ref Utils "Utils" are a set of utility classes that provide a set of miscellaneous
 services. 
 
 - @ref UnitTesting "Unit Testing" components provide a set of basic automatic checks
 for the user developed classes.
 
 @page Data Data classes
 
 The data classes are used for template instantiation, and hence they
 have no actual code. They serve for storing the following information:
 
 - @anchor Input @a Input: input data of the problem.  
 - @anchor Output @a Output: output as it has to be delivered to the user.
 - @anchor State @a State: an element of the search space.
 - @anchor Move @a Move: a local move. The class move needs to have the operators == and < defined.
 - @anchor CFType @a CFType: the cost function type. This template is by default
   instantiated to @c int, representing an integer (discrete) cost function.
   However it can be redefined by the user to allow for real-valued cost 
   functions or cost functions of arbitrary types (provided that the basic
   arithmetic and comparison operators will be defined).
 
 In a few applications, @a State and @a Output classes may
 coincide but, in general, the @e search @e space, -which is explored
 by the algorithm- is only an indirect (not necessarily complete)
 representation of the @e output @e space -which is related to the
 problem specification. 	
 */


