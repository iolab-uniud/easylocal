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

#if !defined(_DELTA_COST_COMPONENT_HH_)
#define _DELTA_COST_COMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <stdexcept>

/** A class for managing the variations of a single component of the cost function. Some
 of the methods are MustDef.
 
 @ingroup Helpers
 */
template <class Input, class State, class Move, typename CFtype = int>
class DeltaCostComponent
{
public:
  /** 
    Prints out the current state of the DeltaCostComponent.
    @param os the output stream to which the state is printed out, it defaults to the standard output.
	 */
  void Print(std::ostream& os = std::cout) const;

  /** 
    Returns the CostComponent associated with the DeltaCostComponent object.
    @return the @ref CostComponent.
	 */
  CostComponent<Input,State,CFtype>& GetCostComponent() const { return cc; }
  
  bool IsHard() const { return cc.IsHard(); }
  
  bool IsSoft() const { return cc.IsSoft(); }

  virtual CFtype DeltaCost(const State& st, const Move& mv) const;

  bool IsDeltaImplemented() const { return true; }

  /** A symbolic name of the DeltaCostComponent. */
  const std::string name; 

protected:
  /**
   @brief Constructs a DeltaCostComponent providing an input object, the related CostComponent and a name.
	 @param in an Input object.
	 @param cc a related CostComponent.
	 @param name the name assigned to the object.
   */
  DeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc, std::string name);
  
  /**
	 @brief This method computes the variation of the cost on a given @ref State due to a specific @ref Move.
	 @param st the starting State upon which the variation of the cost has to be computed.
	 @param mv the Move which would be applied to the State st in order to compute the variation.
	 @return the cost variation by applying Move mv on State st.
	 */
  virtual CFtype ComputeDeltaCost(const State& st, const Move& mv) const = 0;

  /** The @ref Input object */
  const Input& in;

  /** The @ref CostComponent associated with the DeltaCostComponent. */
  CostComponent<Input,State,CFtype>& cc;

};

/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, typename CFtype>
DeltaCostComponent<Input,State,Move,CFtype>::DeltaCostComponent(const Input& i, 
  CostComponent<Input,State,CFtype>& e_cc, std::string name)
  : name(name), in(i), cc(e_cc)
{
}

template <class Input, class State, class Move, typename CFtype>
void DeltaCostComponent<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os << "  DeltaCost Component: " + this->GetName() << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
CFtype DeltaCostComponent<Input,State,Move,CFtype>::DeltaCost(const State& st, 
  const Move& mv) const
{
  return this->cc.Weight() * ComputeDeltaCost(st, mv);
}  


#endif // define _DELTACOSTCOMPONENT_HH_
