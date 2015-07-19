#ifndef _CHANGE_HH
#define _CHANGE_HH

#include "expression.hh"

namespace EasyLocal {
  namespace Modeling {

 // TODO: find a more meaningful name
    template <typename T>
    class Change : public Core::Printable
    {
    public:
        Change(const std::shared_ptr<Var<T>>& var, const T& val) :
    };

    /** A tentative Change composed by a single assignment of a decision variable. */
    template <typename T>
    class UnitChange
    {
    public:

      UnitChange() : val(0)
      {}

      /** Constructor.
       @param var the variable to modify
       @param val the value to assign to the variable
       */
      UnitChange(const Var<T>& var, T val) : var(var), val(val)
      {}

      /** @copydoc Printable::print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Change: " << var << "<<=" << val;
      }

      /** The variable to assign */
      Var<T> var;

      /** The value to assign to the variable */
      T val;
    };

    /** Assignment operator for BasicChange (generates a BasicChange from a Var and a value). */
    template <typename T>
    UnitChange<T> operator<<=(const Var<T>& var, const T& val)
    {
      return UnitChange<T>(var, val);
    }

    /** A tentative Change composed by a multiple assignments to decision variables. */
    template <typename T>
    class CompositeChange : public std::vector<BasicChange<T>>
    {
    public:
      /** Constructor.
       @param mv BasicChange on which this Composite Change is initialized */
      CompositeChange(const BasicChange<T>& mv)
      {
        this->push_back(mv);
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void print(std::ostream& os) const
      {
        for (const BasicChange<T>& mv : (*this))
        {
          mv.Print(os);
          os << " ";
        }
      }
    };

    /** Composition operator for CompositeChange (builds a CompositeChange from a CompositeChange and a BasicChange). */
    template <typename T>
    CompositeChange<T> operator&&(const CompositeChange<T>& mv1, const BasicChange<T>& mv2)
    {
      CompositeChange<T> m(mv1);
      m.push_back(mv2);
      return m;
    }

    /** Composition operator for CompositeChange (builds a CompositeChange from two BasicChanges). */
    template <typename T>
    CompositeChange<T> operator&&(const BasicChange<T>& mv1, const BasicChange<T>& mv2)
    {
      CompositeChange<T> m(mv1);
      m.push_back(mv2);
      return m;
    }
  }
}

#endif
