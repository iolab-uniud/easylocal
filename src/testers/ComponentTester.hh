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

#if !defined(_COMPONENT_TESTER_HH_)
#define _COMPONENT_TESTER_HH_

#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <utils/Chronometer.hh>

/** The Abstract Move Tester is an interface for a tester that handles
    moves.
    @ingroup Testers
*/  
template <class Input, class Output, class State, typename CFtype = int>
class ComponentTester
{
public:
    /** The method executes the interactions with the test menu on a given state.
    @param st the state */
    virtual void RunMainMenu(State& st) = 0;
    /** The method shall print the menu on a given state. */
    virtual void ShowMenu() = 0;
    /** The method shall execute the choice given by the variable choice,
    @return true if state has been changed
    @param st the state */    
    virtual bool ExecuteChoice(State& st) = 0;
    const std::string name;
protected:
  ComponentTester(std::string name);
    /** Virtual destructor. */
    virtual ~ComponentTester() {}
public:
  unsigned int modality;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs an abstract tester for components and assign it a name passed 
   as parameter.
   @param nm the name of the tester
   @param sm a pointer to a state manager
   @param om a pointer to an output manager
   @param in a pointer to an input object (NULL for default(
*/
template <class Input, class Output, class State, typename CFtype>
ComponentTester<Input,Output,State,CFtype>::ComponentTester(std::string e_name)
  : name(e_name), modality(1) {}


#endif // _COMPONENT_TESTER_HH_
