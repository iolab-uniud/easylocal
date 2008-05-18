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
#include <helpers/ShiftingPenaltyManager.hh>
#include <stdexcept>

// // forward class tag declaration
// template <class Input, class State, class Move, typename CFtype>
// class NeighborhoodExplorer;

/**
 @brief This class handles a shifted cost value as modified by the ShiftingPenaltyManager.
 
 A ShiftedResult object contains both the actual value of the cost function variation, as
 computed by a DeltaCostComponent object, and its shifted_value.
 */

template <typename CFtype>
class ShiftedResult
{
public:
	/** @brief Creates an empty ShiftedResult object, whose actual and shifted values are both zero. */
  ShiftedResult() : actual_value(0), shifted_value(0.0) {}
  CFtype actual_value; /**< The actual value of the cost function variation, as computed by a DeltaCostComponent. */
  double shifted_value; /**< The shifted value of the cost function variation, modified by a ShiftingPenaltyManager. */
};

/** @brief Sum operator for two @ref ShiftedResult "ShiftedResult"s. */
template <typename CFtype>
ShiftedResult<CFtype> operator+(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2);

/** @brief Subtraction operator for two @ref ShiftedResult "ShiftedResult"s. */
template <typename CFtype>
ShiftedResult<CFtype> operator-(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2);

/** @brief Multiplication operator for a ShiftedResult. */
template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const Multype& mul, const ShiftedResult<CFtype>& sr);

/** @brief Multiplication operator for a ShiftedResult. */
template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const ShiftedResult<CFtype>& sr, const Multype& mul);

/** 
 @brief The abstract class for managing the variations of a single component of the
 cost function.
 
 This class contains the abstract interface of the DeltaCostComponents, whose purpose
 is to compute the variations of a single component of the cost function.
*/

template <class Input, class State, class Move, typename CFtype = int>
class AbstractDeltaCostComponent
{
public:
  /** 
	 @brief Returns a logical value stating whether the computation of the cost variation is performed efficiently or by actually performing the
	 move and computing the difference of cost in two states.
	 @returns true if the concrete class implements an efficient computation of the cost variation, false otherwise.
	 */
  virtual bool IsDeltaImplemented() const = 0;
  /**
	 @brief Returns a logical value stating whether the CostComponent object that corresponds to the current DeltaCostComponent
	 is a @a hard cost component (i.e., a constraint violation).
	 @returns true if the concrete class is attached to a hard cost component, false otherwise.
	 */
  virtual bool IsHard() const = 0;  
  /**
	 @brief Returns a logical value stating whether the CostComponent object that corresponds to the current DeltaCostComponent
	 is a @a soft cost component (i.e., a violation of a preference).
	 @returns true if the concrete class is attached to a soft cost component, false otherwise.
	 */
  virtual bool IsSoft() const = 0;
  /** 
	 @brief Reads the parameters of the DeltaCostComponent by interacting with a pair of input/output stream objects. 
	 @param is the input stream from which the parameters are read (it defaults to the standard input).
	 @param os the output stream to which the prompts for the parameter are written (it defaults to the standard output).
	 */
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout) = 0;
  /**
	  @brief A notification method, which asks the attached @ref ShiftingPenaltyManager "ShiftingPenaltyManager"s
	  to reset their shifted value.
	 */
  virtual void ResetShift() = 0;
  /**
	 @brief A notification method, which asks the attached @ref ShiftingPenaltyManager "ShiftingPenaltyManager"s
	 to update their shifted value according to the State passed as parameter.
	 @param st the State object for which the shifted value has to be modified.
	 */
  virtual void UpdateShift(const State& st) = 0;  
  const std::string name; /**< A symbolic name of the DeltaCostComponent. */
protected:
  /**
	 @brief Constructs an empty AbstractDeltaCostComponent assigning it a give name.
	 @param e_name the symbolic name of the DeltaCostComponent.
	 */
  AbstractDeltaCostComponent(std::string e_name) : name(e_name) {}  
  virtual ~AbstractDeltaCostComponent() {}
};

/**
 @brief A more concrete class for managing the variations of a single component of the
 cost function.
 
 It is a direct subclass of AbstractDeltaCostComponent and it adds a boolean variable
 template for handling the two cases implemented in the two direct subclasses:
  
 - FilledDeltaCostComponent: a DeltaCostComponent for which an implementation of the
 ComputeDeltaCost() function is provided by the user (in that case @c is_delta_implemented is @c true).
 - EmptyDeltaCostComponent: a DeltaCostComponent for which a default implementation
 of the ComputeDeltaCost() function is provided in the framework by computing the difference
 of costs in current state and in the neighbor state (in that case @c is_delta_implemented is @c false).
 
  @ingroup Helpers
 */
template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype = int>
class DeltaCostComponent
  : public AbstractDeltaCostComponent<Input,State,Move,CFtype>
{
public:
  /**
	 @brief Prints out the current state of the DeltaCostComponent.
	 @param os the output stream to which the state is printed out, it defaults to the standard output.
	 */
  void Print(std::ostream& os = std::cout) const;
  /**
	 @brief Sets the current shifting penalty manager to the one passed as parameter.
	 @param spm a ShiftingPenaltyManager assigned to the DeltaCostComponent object.
   */
  void SetShiftingPenaltyManager(ShiftingPenaltyManager<CFtype>& spm);
	/**
	 Unsets the shifting penalty manager.
	*/
  void UnsetShiftingPenaltyManager();
  virtual void ResetShift();
  virtual void UpdateShift(const State& st);
  virtual bool IsDeltaImplemented() const  { return is_delta_implemented; }
  /** 
	 @brief Returns the CostComponent associated with the DeltaCostComponent object.
	 @returns the CostComponent associated with the DeltaCostComponent object.
	 */
  CostComponent<Input,State,CFtype>& GetCostComponent() const { return cc; }
  virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  bool IsHard() const { return cc.IsHard(); }
  bool IsSoft() const { return cc.IsSoft(); }
protected:
  /**
   @brief Constructs a DeltaCostComponent providing an input object, the related CostComponent and a name.
	 @param in an Input object.
	 @param cc a related CostComponent.
	 @param name the name assigned to the object.
   */
  DeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc, std::string name);
  /**
   @brief Constructs a DeltaCostComponent which is equipped with a ShiftingPenaltyManager for the enabling the shifting penalty strategy.
	 @param in an Input object.
	 @param cc a related CostComponent.
	 @param spm a ShiftingPenaltyManager.
	 @param name the name assigned to the object.
	 */
  DeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc, ShiftingPenaltyManager<CFtype>& spm, std::string name);
  /**
	 @brief This method computes the variation of the cost on a given @ref State due to a specific @ref Move.
	 @param st the starting State upon which the variation of the cost has to be computed.
	 @param mv the Move which would be applied to the State st in order to compute the variation.
	 @return the cost variation by applying Move mv on State st.
	 */
  virtual CFtype ComputeDeltaCost(const State& st, const Move& mv) const = 0;
  const Input& in; /**< The @ref Input object */
  CostComponent<Input,State,CFtype>& cc; /**< @brief The CostComponent associated with the DeltaCostComponent. */
  ShiftingPenaltyManager<CFtype>* spm; /**< @brief The ShiftingPenaltyManager attached to the DeltaCostComponent. */
  bool is_shifted; /**< @brief It has value true if the shifting-penalty mechanism is enabled. */
};

/**
 @brief A DeltaCostComponent for which an efficient implementation of the ComputeDeltaCost method is provided by the user.
 
 The user who is using this class is required to implement the virtual method ComputeDeltaCost() in order to compute
 the variations of the cost function on a given State due to a Move. In principle the user should try to avoid
 performing the Move (a costly operation, in general) but he/she has just to simulate or reason about it.  
 
  @ingroup Helpers
 */
template <class Input, class State, class Move, typename CFtype = int>
class FilledDeltaCostComponent
: public DeltaCostComponent<Input,State,Move,true,CFtype>
{
public:
  /** 
	 @brief Inovokes the virtual method ComputeDeltaCost() for computing the cost variation.
	 @param st the starting State upon which the variation of the cost has to be computed.
	 @param mv the Move which would be applied to the State st in order to compute the variation.
	 @return the cost variation by applying Move mv on State st.
	 */
  virtual CFtype DeltaCost(const State& st, const Move& mv) const;
  /**
	 @brief Inovokes the virtual method ComputeDeltaCost() for computing the cost variation and embeds it in a ShiftedResult
	 object. In addition the function asks the ShiftingPenaltyManager to compute also the shifted value of the cost 
	 variation.
	 @param st the starting State upon which the variation of the cost has to be computed.
	 @param mv the Move which would be applied to the State st in order to compute the variation.	 
	 @return the cost variation and the shifted cost variation by applying Move mv on State st.
   */
  virtual ShiftedResult<CFtype> DeltaShiftedCost(const State& st, const Move& mv) const;
protected:
  FilledDeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc, std::string name)
    : DeltaCostComponent<Input,State,Move,true,CFtype>(in,cc,name) {}
  FilledDeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc, ShiftingPenaltyManager<CFtype>& spm, std::string name)
    : DeltaCostComponent<Input,State,Move,true,CFtype>(in,cc,spm,name) {}
};

/**
 @brief A DeltaCostComponent with a default implementation of the computation of the cost variation.
 
 The user who is using this class is only required to instantiate the templates to obtain a workable object.
 The computation of the cost variation is performed by a difference of the cost function on the final State 
 and on the starting state (i.e., f(st1) - f(st)). This out-of-the-box implementation can be
 very inefficient in some cases.
 
 @ingroup Helpers
 
 */

template <class Input, class State, class Move, typename CFtype = int>
class EmptyDeltaCostComponent
: public DeltaCostComponent<Input,State,Move,false,CFtype>
{
public:
  EmptyDeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc,  std::string name)
  : DeltaCostComponent<Input,State,Move,false,CFtype>(in,cc,name)   {}
  EmptyDeltaCostComponent(const Input& in, CostComponent<Input,State,CFtype>& cc,ShiftingPenaltyManager<CFtype>& spm,  std::string name)
  : DeltaCostComponent<Input,State,Move,false,CFtype>(in,cc,spm,name)   {}
  /**
   @brief Computes the cost variation by moving from the first State to the second one, i.e. f(st1) - f(st).
   @param st the starting State upon which the variation of the cost has to be computed.
   @param st1 the final State upon which the variation of the cost has to be computed.
   @return the cost variation and the shifted cost variation by moving from State st to State st1.
   */
  virtual CFtype DeltaCost(const State& st, const State& st1) const; 
  /**
   @brief Computes the cost variation by moving from the first State to the second one, i.e. f(st1) - f(st), and embeds it in a ShiftedResult
	 object. In addition the function asks the ShiftingPenaltyManager to compute also the shifted value of the cost 
	 variation.	 
   @param st the starting State upon which the variation of the cost has to be computed.
   @param st1 the final State upon which the variation of the cost has to be computed.
	 @return the cost variation and the shifted cost variation by applying Move mv on State st.
   */
  virtual ShiftedResult<CFtype> DeltaShiftedCost(const State& st, const State& st1) const;
  /** 
	 @brief This method is implemented only to make the EmptyDeltaCostComponent a concrete class (without pure virtual methods). However in
	 the machinery it would never be called.
	 */
  CFtype ComputeDeltaCost(const State& st, const Move& mv) const 
  { throw std::logic_error("This method should never be called because the implementation is missing"); }
};


/*************************************************************************
 * Implementation
 *************************************************************************/

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::DeltaCostComponent(const Input& i,
							 CostComponent<Input,State,CFtype>& e_cc, std::string name)
  : AbstractDeltaCostComponent<Input,State,Move,CFtype>(name), in(i), cc(e_cc), spm(NULL)
{
  is_shifted = false;
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::DeltaCostComponent(const Input& i,
							 CostComponent<Input,State,CFtype>& e_cc, ShiftingPenaltyManager<CFtype>& a_spm, std::string name)
  : AbstractDeltaCostComponent<Input,State,Move,CFtype>(name), in(i), cc(e_cc), spm(&a_spm)
{
  is_shifted = true;
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::
       SetShiftingPenaltyManager(ShiftingPenaltyManager<CFtype>& a_spm)
{
  spm = &a_spm;
  is_shifted = true;
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::
       UnsetShiftingPenaltyManager()
{
  spm = NULL;
  is_shifted = false;
}




template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::Print(std::ostream& os) const
{
  os << "  DeltaCost Component: " + this->GetName() << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
CFtype FilledDeltaCostComponent<Input,State,Move,CFtype>::DeltaCost(const State& st, const Move& mv) const
{
    return this->cc.Weight() * ComputeDeltaCost(st, mv);
}  

template <class Input, class State, class Move, typename CFtype>
ShiftedResult<CFtype> FilledDeltaCostComponent<Input,State,Move,CFtype>::DeltaShiftedCost(const State& st,
						       const Move& mv) const
{
  ShiftedResult<CFtype> res;
  res.actual_value = this->cc.Weight() * ComputeDeltaCost(st, mv);
  if (this->is_shifted)
    res.shifted_value = res.actual_value * this->spm->Shift();
  else
    res.shifted_value = (double)res.actual_value;

  return res;
}  

/**
   tentative definition, to be replaced with the actual (much more
   efficient) implementation
*/

template <class Input, class State, class Move, typename CFtype>
CFtype EmptyDeltaCostComponent<Input,State,Move,CFtype>::DeltaCost(const State& st,
						       const State& st1) const
{ 
  return this->cc.Weight() * (this->cc.ComputeCost(st1) - this->cc.ComputeCost(st));
}

template <class Input, class State, class Move, typename CFtype>
ShiftedResult<CFtype> EmptyDeltaCostComponent<Input,State,Move,CFtype>::DeltaShiftedCost(const State& st,
						       const State& st1) const
{
  ShiftedResult<CFtype> res;
  res.actual_value = this->cc.Weight() * (this->cc.ComputeCost(st1) - this->cc.ComputeCost(st));
  if (this->is_shifted)
    res.shifted_value =  res.actual_value * this->spm->Shift();
  else
    res.shifted_value =  (double)res.actual_value;

  return res;
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::ResetShift()
{
  if (is_shifted)
    {
      bool reset;
      reset = spm->Reset();
    }
}

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::UpdateShift(const State& st)
{
  if (is_shifted)
    {
      bool update;
      update = spm->Update(cc.Cost(st));
    }
}   

template <class Input, class State, class Move, bool is_delta_implemented, typename CFtype>
void DeltaCostComponent<Input,State,Move,is_delta_implemented,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  if (is_shifted)
    spm->ReadParameters(is,os);
}


template <typename CFtype>
ShiftedResult<CFtype> operator+(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2)
{
  ShiftedResult<CFtype> res = sr1;
  res.actual_value += sr2.actual_value;
  res.shifted_value += sr2.shifted_value;

  return res;
}

template <typename CFtype>
ShiftedResult<CFtype> operator-(const ShiftedResult<CFtype>& sr1, const ShiftedResult<CFtype>& sr2)
{
  ShiftedResult<CFtype> res = sr1;
  res.actual_value -= sr2.actual_value;
  res.shifted_value -= sr2.shifted_value;

  return res; 
}

template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const Multype& mul, const ShiftedResult<CFtype>& sr)
{
  ShiftedResult<CFtype> res = sr;
  res.actual_value *= mul;
  res.shifted_value *= mul;

  return res;
}

template <typename CFtype, typename Multype>
ShiftedResult<CFtype> operator*(const ShiftedResult<CFtype>& sr, const Multype& mul)
{  
  ShiftedResult<CFtype> res = sr;
  res.actual_value *= mul;
  res.shifted_value *= mul;

  return res;
}


#endif // define _DELTACOSTCOMPONENT_HH_
