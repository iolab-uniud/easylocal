#if !defined(_CHANGE_HH_)
#define _CHANGE_HH_


#include "easylocal/modeling/expression.hh"

namespace EasyLocal {
    
  namespace Modeling {
        
    // TODO: find a more meaningful name    
    template <typename T>
    class Change : public Printable
    {
    public:
    };
  
    /** A tentative Change composed by a single assignment of a decision variable. */
    template <typename T>
    class BasicChange
    {
    public:
      /** Constructor.
      @param var the variable to modify
      @param val the value to assign to the variable
      */
      BasicChange(const Var<T>& var, const T& val) : var(var), val(val)
        { }
      
      /** @copydoc Printable::print(std::ostream&) */
      virtual void print(std::ostream& os) const
      {
        os << "Change: " << var << "<<=" << val;
      }
      
      /** A reference to the variable to assign */
      const Var<T>& var;
      
      /** A reference to the value to assign to the variable */
      const T& val;
    };
    
    /** Assignment operator for BasicChange (generates a BasicChange from a Var and a value). */
    template <typename T>
    BasicChange<T> operator<<=(const Var<T>& var, const T& val)
    {
      return BasicChange<T>(var, val);
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
      
      /** @copydoc Printable::print(std::ostream&) */
      virtual void print(std::ostream& os) const
      {
        for (const BasicChange<T>& mv : (*this))
        {
          mv.print(os);
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

#endif // _CHANGE_HH_