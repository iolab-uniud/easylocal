#if !defined(_DELTA_COST_COMPONENT_HH_)
#define _DELTA_COST_COMPONENT_HH_

#include <stdexcept>

#include "helpers/CostComponent.hh"

/** A class for managing the variations of a single component of the cost function. Some
 of the methods are MustDef.
 
 @ingroup Helpers
 */
template <class Input, class State, class Move, typename CFtype>
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

  virtual bool IsDeltaImplemented() const { return true; }

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
  os << "  DeltaCost Component: " + this->name << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
CFtype DeltaCostComponent<Input,State,Move,CFtype>::DeltaCost(const State& st, 
  const Move& mv) const
{
  return this->cc.Weight() * ComputeDeltaCost(st, mv);
}

// forward class declaration
template <class Input, class State, class Move, typename CFtype>
class NeighborhoodExplorer;

/** An adapter class for using a cost component in place of a delta cost component.
 It is used by the neighborhood explorer to wrap the unimplemented deltas.
 @ingroup Helpers
 */
template <class Input, class State, class Move, typename CFtype>
class DeltaCostComponentAdapter : public DeltaCostComponent<Input, State, Move, CFtype>
{
public:
  DeltaCostComponentAdapter(const Input& in, CostComponent<Input,State,CFtype>& cc, NeighborhoodExplorer<Input, State, Move, CFtype>& ne);
  virtual bool IsDeltaImplemented() const { return false; }
protected:
  virtual CFtype ComputeDeltaCost(const State& st, const Move& mv) const
  {
    State new_st = st;
    ne.MakeMove(new_st, mv);
    return this->cc.ComputeCost(new_st) - this->cc.ComputeCost(st);
  }
  NeighborhoodExplorer<Input, State, Move, CFtype>& ne;
};

template <class Input, class State, class Move, typename CFtype>
DeltaCostComponentAdapter<Input, State, Move, CFtype>::DeltaCostComponentAdapter(const Input& in,
                                                                                 CostComponent<Input,State,CFtype>& cc,
                                                                                 NeighborhoodExplorer<Input, State, Move, CFtype>& ne)
: DeltaCostComponent<Input, State, Move, CFtype>(in, cc, "UDelta" + cc.name), ne(ne)
{}

#endif // _DELTA_COST_COMPONENT_HH_