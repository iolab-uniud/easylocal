#pragma once

#include <iostream>
#include <vector>
#include "helpers/coststructure.hh"
#include "utils/deprecationhandler.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** The responsibility of this class is to compute a component of cost based on the information contained in a state. It doesn't handle delta costs (i.e., variations of the cost functions due to a move), as they are treated in @ref DeltaCostComponent.
     All cost components for a given (@Input, @State) pair are statically registered in the system and they are accessible with an index.
     @brief The class CostComponent manages one single component of the cost, either hard or soft.
     @tparam Input the class representing the problem input
     @tparam State the class representing the problem's state
     @tparam CFtype the type of the cost function (typically int)
     @ingroup Helpers
     */
    template <class Input, class State, class CFtype = int>
    class CostComponent : protected DeprecationHandler<Input>
    {
    public:
      /** @copydoc Printable::Print() */
      virtual void Print(std::ostream &os = std::cout) const;
      
      /** Old-style method, without Input
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      CFtype ComputeCost(const State &st) const
      {
        return ComputeCost(this->GetInput(), st);
      }
      
      /** Computes this component of cost with respect to a given state not considering its weight.
       @param in the @ref Input object
       @param st the @ref State to be evaluated
       @return the computed cost, regardless of its weight
       */
      virtual CFtype ComputeCost(const Input& in, const State &st) const = 0;
      
      /** Old-style method, without Input
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      CFtype Cost(const State &st) const
      {
        return Cost(this->GetInput(), st);
      }
      
      
      /** Computes this component of cost with respect to a given state.
       @param in the @ref Input object
       @param st the @ref State to be evaluated
       @return the computed cost, multiplied by its weight
       @remarks internally calls @ref ComputeCost and multiplies the result by the weight of the cost component.
       */
      CFtype Cost(const Input& in, const State &st) const
      {
        return Weight(in) * ComputeCost(in, st);
      }
      
      /** Old-style method, without Input
       @deprecated
       */
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      void PrintViolations(const State &st, std::ostream &os = std::cout) const
      {
        PrintViolations(this->GetInput(), st, os);
      }
      
      /** Prints the violations relative to this cost component with respect to the specified state.
       @param in the @Input object
       @param st the @State to be evaluated
       @param os the output stream where the description has to be printed
       */
      virtual void PrintViolations(const Input& in, const State &st, std::ostream &os = std::cout) const = 0;                  
      
      /** Gets the weight of this cost component.
       @return the weight of this cost component
       */
      virtual CFtype Weight(const Input& in) const = 0;      
      
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
      
      /** Constructor.
       @param weight weight of the cost component
       @param is_hard a flag which tells if the cost component is hard or soft
       @param name name of the cost component (for debug reasons)
       */
      CostComponent(bool is_hard, std::string name);
      
      /** Destructor. */
      virtual ~CostComponent()
      {}
      
      const size_t hash;
      
    protected:
      
      [[deprecated("This is the old style easylocal interface, it is mandatory to upgrade to Input-less class and Input-aware methods")]]
      CostComponent(const Input &in, const CFtype &weight, bool is_hard, std::string name) : DeprecationHandler<Input>(in), name(name), hash(std::hash<std::string>()(typeid(this).name() + name)), is_hard(is_hard)
      {}
      
      /** Flag that tells if the cost component is soft or hard */
      bool is_hard;      
    };
    
    /** IMPLEMENTATION */
    
    template <class Input, class State, typename CFtype>
    CostComponent<Input, State, CFtype>::CostComponent(bool is_hard, std::string name)
    : name(name), hash(std::hash<std::string>()(typeid(this).name() + name)), is_hard(is_hard)
    {}
    
    template <class Input, class State, typename CFtype>
    void CostComponent<Input, State, CFtype>::Print(std::ostream &os) const
    {
      os << "Cost Component " << name << ": " << (is_hard ? "*" : "") << std::endl;
    }
  } // namespace Core
} // namespace EasyLocal
