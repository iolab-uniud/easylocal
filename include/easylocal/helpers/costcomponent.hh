#if !defined(_COST_COMPONENT_HH_)
#define _COST_COMPONENT_HH_

#include <iostream>
#include "easylocal/utils/printable.hh"

namespace EasyLocal {

  namespace Core {

    /** The responsibility of this class is to compute a component of cost based on the information contained in a state. It doesn't handle delta costs, as they are treated in @ref DeltaCostComponent.
        @brief The class CostComponent manages one single component of the cost, either hard or soft.
        @tparam Input the class representing the problem input
        @tparam State the class representing the problem's state
        @tparam CFtype the type of the cost function (typically int)
        @ingroup Helpers
    */
    template <class Input, class State, typename CFtype>
    class CostComponent : public Printable
    {
    public:

      /** @copydoc Printable::Print() */
      virtual void Print(std::ostream& os = std::cout) const;

      /** Compute this component of cost with respect to a given state and regardless of the weight.
          @param st the @ref State to be evaluated
          @return the computed cost, regardless of its weight
      */
      virtual CFtype ComputeCost(const State& st) const = 0;

      /** Compute this component of cost with respect to a given state.
          @param st the @ref State to be evaluated
          @return the computed cost, multiplied by its weight
          @remarks internally calls @ref ComputeCost and multiplies the result for the weight of the cost component.
      */
      CFtype Cost(const State& st) const { return weight * ComputeCost(st); }

      /** Print the violations relative to this cost component with respect to the specified state.
          @param st the @State to be evaluated
          @param os the output stream where the description has to be printed
      */
      virtual void PrintViolations(const State& st, std::ostream& os = std::cout) const = 0;

      /** Get the weight of this cost component.
          @return the weight of this cost component
      */
      CFtype Weight() const { return weight; }

      /** Set a new weight for this cost component.
          @param w the new weight
      */
      void SetWeight(const CFtype& w) { weight = w; }

      /** Set this cost component to be hard. */
      void SetHard() { is_hard = true; }

      /** Set this cost component to be soft. */
      void SetSoft() { is_hard = false; }

      /** Tell if this cost component is hard.
          @return true if this cost component is hard, false otherwise
      */
      bool IsHard() const { return is_hard; }

      /** Tell if this cost component is soft.
          @return true if this cost component is soft, false otherwise
      */
      bool IsSoft() const { return !is_hard; }

      /** Name of this cost component (for debug). */
      const std::string name;

    protected:

      /** Constructor.
          @param in @ref Input object
          @param weight weight of the cost component
          @param hard a flag which tells if the cost component is hard or soft
          @param name name of the cost component (for debug reasons)
      */
      CostComponent(const Input& in, const CFtype& weight, bool hard, std::string name);

      /** Destructor. */
      virtual ~CostComponent() {}

      /** Input object. */
      const Input& in;

      /** Weight of the cost component. */
      CFtype weight;

      /** Flag that tells if the cost component is soft or hard */
      bool is_hard;
    };

    /** IMPLEMENTATION */

    template <class Input, class State, typename CFtype>
    CostComponent<Input, State, CFtype>::CostComponent(const Input& i, const CFtype& w, bool hard, std::string e_name)
    : name(e_name), in(i), weight(w), is_hard(hard)
    { }

    template <class Input, class State, typename CFtype>
    void CostComponent<Input, State, CFtype>::Print(std::ostream& os) const
    {
      os  << "Cost Component " << name << ": weight " << weight << (is_hard ? "*" : "") << std::endl;
    }

  }
}

#endif // _COST_COMPONENT_HH_
