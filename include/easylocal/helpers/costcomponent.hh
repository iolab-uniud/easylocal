#if !defined(_COST_COMPONENT_HH_)
#define _COST_COMPONENT_HH_

#include <iostream>
#include <vector>
#include "easylocal/utils/printable.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The responsibility of this class is to compute a component of cost based on the information contained in a state. It doesn't handle delta costs (i.e., variations of the cost functions due to a move), as they are treated in @ref DeltaCostComponent.
     All cost components for a given (@Input, @State) pair are statically registered in the system and they are accessible with an index.
     @brief The class CostComponent manages one single component of the cost, either hard or soft.
     @tparam Input the class representing the problem input
     @tparam State the class representing the problem's state
     @tparam CFtype the type of the cost function (typically int)
     @ingroup Helpers
     */
    template <class Input, class State, typename CFtype = int>
    class CostComponent : public Printable
    {
      static size_t last_index;
    public:
      
      /** @copydoc Printable::Print() */
      virtual void Print(std::ostream& os = std::cout) const;
      
      /** Computes this component of cost with respect to a given state not considering its weight.
       @param st the @ref State to be evaluated
       @return the computed cost, regardless of its weight
       */
      virtual CFtype ComputeCost(const State& st) const = 0;
      
      /** Computes this component of cost with respect to a given state.
       @param st the @ref State to be evaluated
       @return the computed cost, multiplied by its weight
       @remarks internally calls @ref ComputeCost and multiplies the result by the weight of the cost component.
       */
      CFtype Cost(const State& st) const { return weight * ComputeCost(st); }
      
      /** Prints the violations relative to this cost component with respect to the specified state.
       @param st the @State to be evaluated
       @param os the output stream where the description has to be printed
       */
      virtual void PrintViolations(const State& st, std::ostream& os = std::cout) const = 0;
      
      /** Gets the weight of this cost component.
       @return the weight of this cost component
       */
      CFtype Weight() const { return weight; }
      
      /** Sets a new weight for this cost component.
       @param w the new weight
       */
      void SetWeight(const CFtype& w) { weight = w; }
      
      /** Sets this cost component to be hard. */
      void SetHard() { is_hard = true; }
      
      /** Sets this cost component to be soft. */
      void SetSoft() { is_hard = false; }
      
      /** Tells whether this cost component is a hard cost component.
       @return true if this cost component is hard, false otherwise
       */
      bool IsHard() const { return is_hard; }
      
      /** Tells if this cost component is soft a soft cost component.
       @return true if this cost component is soft, false otherwise
       */
      bool IsSoft() const { return !is_hard; }
      
      /** Name of this cost component (for debug). */
      const std::string name;      
      
      /** Returns the index of the cost component as it has been registered in the system for the given combination (@Input, @State).
       * @return the index of the cost component
       */
      size_t Index() const
      {
        return index;
      }
      
      /** Returns the total number of cost components registered in the system for the given combination (@Input, @State).
      @return the number of cost components
       */
      static const size_t CostComponents()
      {
        return last_index;
      }

      
      /** Returns the i-th cost component registered in the system for the given combination (@Input, @State).
       * @return a reference to the i-th cost copmponent
       */
      static const CostComponent<Input, State, CFtype>& Component(size_t i)
      {
        return *cost_components[i];
      }
    protected:
      
      /** Constructor.
       @param in @ref Input object
       @param weight weight of the cost component
       @param is_hard a flag which tells if the cost component is hard or soft
       @param name name of the cost component (for debug reasons)
       */
      CostComponent(const Input& in, const CFtype& weight, bool is_hard, std::string name);
      
      /** Destructor. */
      virtual ~CostComponent() {}
      
      /** Input object. */
      const Input& in;
      
      /** Weight of the cost component. */
      CFtype weight;
      
      /** Flag that tells if the cost component is soft or hard */
      bool is_hard;            
      
      
    protected:
      /** The overall index of this cost components */
      const size_t index;
      
      /** The registered cost components */
      static std::vector<CostComponent*> cost_components;
    };
    
    
    
    /** IMPLEMENTATION */
    
    template <class Input, class State, typename CFtype>
    CostComponent<Input, State, CFtype>::CostComponent(const Input& in, const CFtype& weight, bool is_hard, std::string name)
    : name(name), in(in), weight(weight), is_hard(is_hard), index(last_index++)
    {
      cost_components.push_back(this);
    }
    
    template <class Input, class State, typename CFtype>
    void CostComponent<Input, State, CFtype>::Print(std::ostream& os) const
    {
      os  << "Cost Component " << name << ": weight " << weight << (is_hard ? "*" : "") << std::endl;
    }
    
    template <class Input, class State, typename CFtype>
    size_t CostComponent<Input, State, CFtype>::last_index = 0;
    
    template <class Input, class State, typename CFtype>
    std::vector<CostComponent<Input, State, CFtype>*> CostComponent<Input, State, CFtype>::cost_components;
  }
}

#endif // _COST_COMPONENT_HH_
