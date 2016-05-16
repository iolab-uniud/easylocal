#pragma once

#include <stdexcept>

//#include "easylocal/helpers/coststructure.hh"
#include "easylocal/helpers/costcomponent.hh"
#include "easylocal/utils/printable.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** A class for managing the variations of a single component of the cost function. Some of the methods are MustDef.
     @ingroup Helpers
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class DeltaCostComponent : public Printable
    {
    public:
      
      /** Returns the (complete) CostComponent associated with the DeltaCostComponent object.
       @return the @ref CostComponent
       */
      const CostComponent<Input, State, CFtype>& GetCostComponent() const { return cc; }
      
      /** @copydoc Printable::Print() */
      virtual void Print(std::ostream& os = std::cout) const;
      
      /** @copydoc CostComponent::IsHard() */
      bool IsHard() const { return cc.IsHard(); }
      
      /** @copydoc CostComponent::IsSoft() */
      bool IsSoft() const { return cc.IsSoft(); }
      
      /** Returns the variation in the cost function induced by the move according to this cost component.
       @param st state to evaluate
       @param mv move to evaluate
       */
      virtual CFtype DeltaCost(const State& st, const Move& mv) const;
      
      /** Returns whether the delta function is implemented, or the complete cost component is used. */
      virtual bool IsDeltaImplemented() const { return true; }
      
      /** A symbolic name of the DeltaCostComponent. */
      const std::string name;      
      
    protected:
      
      /** Constructor.
       @brief Constructs a DeltaCostComponent providing an input object, the related CostComponent and a name
       @param in an Input object
       @param cc a related CostComponent
       @param name the name assigned to the object
       */
      DeltaCostComponent(const Input& in, CostComponent<Input, State, CFtype>& cc, std::string name);
      
      /** Computes the variation of the cost on a given @ref State due to a specific @ref Move.
       @param st the starting State upon which the variation of the cost has to be computed
       @param mv the Move which would be applied to the State st in order to compute the variation
       @return the cost variation by applying Move mv on State st
       */
      virtual CFtype ComputeDeltaCost(const State& st, const Move& mv) const = 0;
      
      /** The @ref Input object. */
      const Input& in;
      
    public:
      /** The @ref CostComponent associated with the DeltaCostComponent. */
      const CostComponent<Input, State, CFtype>& cc;
      
    };
    
    /** IMPLEMENTATION */
    
    template <class Input, class State, class Move, class CFtype>
    DeltaCostComponent<Input, State, Move, CFtype>::DeltaCostComponent(const Input& i, CostComponent<Input, State, CFtype>& e_cc, std::string name)
    : name(name), in(i), cc(e_cc)
    {}
    
    template <class Input, class State, class Move, class CFtype>
    void DeltaCostComponent<Input, State, Move, CFtype>::Print(std::ostream& os) const
    {
      os << "  DeltaCost Component: " + this->name << std::endl;
    }
    
    template <class Input, class State, class Move, class CFtype>
    CFtype DeltaCostComponent<Input, State, Move, CFtype>::DeltaCost(const State& st,
                                                                     const Move& mv) const
    {
      return this->cc.Weight() * ComputeDeltaCost(st, mv);
    }
    
    template <class Input, class State, class Move, class CostStructure>
    class NeighborhoodExplorer;
    
    /** An adapter class for using a (full) @ref CostComponent in place of a @ref DeltaCostComponent. It is used by the @ref NeighborhoodExplorer to wrap the unimplemented deltas.
     @ingroup Helpers
     */
    template <class Input, class State, class Move, class CostStructure>
    class DeltaCostComponentAdapter : public DeltaCostComponent<Input, State, Move, typename CostStructure::CFtype>
    {
    public:
      typedef typename CostStructure::CFtype CFtype;
      
      /** Constructor. */
      DeltaCostComponentAdapter(const Input& in, CostComponent<Input, State, CFtype>& cc, const NeighborhoodExplorer<Input, State, Move, CostStructure>& ne) : DeltaCostComponent<Input, State, Move, CFtype>(in, cc, "DeltaAdapter" + cc.name), ne(ne) {}
      
      /** @copydoc DeltaCostComponent::IsDeltaImplemented() */
      virtual bool IsDeltaImplemented() const { return false; }
      
    protected:
      
      /** @copydoc DeltaCostComponent::ComputeDeltaCost() */
      virtual CFtype ComputeDeltaCost(const State& st, const Move& mv) const
      {
        State new_st = st;
        ne.MakeMove(new_st, mv);
        return this->cc.ComputeCost(new_st) - this->cc.ComputeCost(st);
      }
      const NeighborhoodExplorer<Input, State, Move, CostStructure>& ne;
    };
  }
}

