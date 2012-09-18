#if !defined(_DELTA_COST_COMPONENT_HH_)
#define _DELTA_COST_COMPONENT_HH_

#include <helpers/CostComponent.hh>
#include <stdexcept>

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


#endif // _DELTA_COST_COMPONENT_HH_
