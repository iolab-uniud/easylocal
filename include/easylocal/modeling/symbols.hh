#ifndef _CEXPRESSION_HH
#define _CEXPRESSION_HH

#include <sstream>
#include <cmath>
#include <memory>
#include <algorithm>
#include <set>

namespace EasyLocal {
  
  namespace Modeling {
    
    /** Forward declaration */
    template <typename T>
    class ExpressionStore;
    
    /** Forward declaration */
    template <typename T>
    class ValueStore;
    
    /** Forward declaration */
    template <typename T>
    class Exp;
    
    /** Template compiled Exp<T>. */
    template <typename T>
    class Sym : public EasyLocal::Core::Printable
    {
    public:
      
      /** Constructor. 
       @param exp_store ExpressionStore into which to register the compiled expression
       */
      Sym(ExpressionStore<T>& exp_store) : exp_store(exp_store)
      {
        depth = 0;
      }
      
    public:
      
      /** Index of the expression in the ExpressionStore */
      size_t index;
      
      /** Parents of the expression in the AST (if any) */
      std::set<size_t> parents;
      
      /** Children of the expression in the AST (if any) */
      std::vector<size_t> children;
      
      /** String representation of the AST item */
      std::string exp;
      
      /** Reference to the ExpressionStore where the compiled expression is registered */
      const ExpressionStore<T>& exp_store;
      
      /** Depth of the expression in the AST */
      unsigned int depth;
      
      /** Computes the value of the expression within ValueStore (from scratch).
       @param st the ValueStore to get the children values from, and store the expression data to
       @param level level to use for the evaluation
       */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const = 0;
      
      /** Computes the value of the expression within ValueStore (delta from previous value).
       @param st the ValueStore to get the children values from, and store the expression data to
       @param level level to use for the evaluation
       */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const = 0;

      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Index: " << index << " " << " p{";
        bool first = true;
        for (auto p : parents)
        {
          if (first)
            first = false;
          else
            os << ", ";
          os << p;
        }
        os << "}, c{";
        first = true;
        for (auto p : children)
        {
          if (first)
            first = false;
          else
            os << ", ";
          os << p;
        }
        os << "} " << exp << " [depth: " << depth << "]";
      }
    };
    
    /** Generic terminal expression.
     @remarks Currently only variable or constant
     */
    template <typename T>
    class TermSym : public Sym<T>
    {
    public:
      using Sym<T>::Sym;
    };

    /** Scalar variable expression. */
    template <typename T>
    class VarSym : public TermSym<T>
    {
    public:
      using TermSym<T>::TermSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {}
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {}
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Var: ";
        Sym<T>::Print(os);
      }
    };
    
    /** Array variable expression. */
    template <typename T>
    class VarArraySym : public TermSym<T>
    {
    public:
      using TermSym<T>::TermSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {}
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {}
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "VarArray: ";
        Sym<T>::Print(os);
      }
      
      /** Size of the variable array (relies on contiguous allocation of array elements) */
      size_t start, size;
    };

    /** Numeric constant expression. */
    template <typename T>
    class ConstSym : public TermSym<T>
    {
    public:
      using TermSym<T>::TermSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        // Note: at first, in the value store the constant value has not been set, so we have to force it
        st.assign(this->index, 0, value);
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {}
      
      /** Value of the constant */
      T value;
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Const: ";
        Sym<T>::Print(os);
      }
    };

    /** Summation expression. */
    template <typename T>
    class SumSym : public Sym<T>
    {
    public:
      using Sym<T>::Sym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        T sum = static_cast<T>(0);
        for (size_t child : this->children)
        {
          sum += st(child, level);
        }
        st.assign(this->index, level, sum);
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        T current_contribution = st(this->index), new_contribution = static_cast<T>(0);
        for (auto child : st.changed_children(this->index, level))
        {
          current_contribution -= st(child);
          new_contribution += st(child, level);
        }
        st.changed_children(this->index, level).clear();
        T value = current_contribution + new_contribution;
        st.assign(this->index, level, value);
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Sum: ";
        Sym<T>::Print(os);
      }
    };

    /** Product expression. */
    template <typename T>
    class MulSym : public Sym<T>
    {
    public:
      using Sym<T>::Sym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        T mul = static_cast<T>(1);
        for (size_t child : this->children)
        {
          T value = st(child, level);
          if (value == static_cast<T>(0))
          {
            mul = static_cast<T>(0);
            break;
          }
          mul *= value;
        }
        st.assign(this->index, level, mul);
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        T current_contribution = st(this->index);
        std::set<size_t>& changed = st.changed_children(this->index, level);
        if (std::any_of(changed.begin(), changed.end(), [&st,&level](const size_t& i)->bool { return st(i, level) == static_cast<T>(0); }))
        {
          st.assign(this->index, level, static_cast<T>(0));
          changed.clear();
          return;
        }
        if (current_contribution == 0)
        {
          this->compute(st, level);
          changed.clear();
          return;
        }
        T new_contribution = static_cast<T>(1);
        for (auto child : changed)
        {
          current_contribution /= st(child);
          new_contribution *= st(child, level);
        }
        changed.clear();
        T value = current_contribution * new_contribution;
        st.assign(this->index, level, value);
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Mul: ";
        Sym<T>::Print(os);
      }
    };

    /** If-then-else expression. */
    template <typename T>
    class IfElseSym : public Sym<T>
    {
    public:
      using Sym<T>::Sym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        if (st(this->children[0], level))
          st.assign(this->index, level, st(this->children[1], level));
        else
          st.assign(this->index, level, st(this->children[2], level));
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        // it is more efficient to directly invoke compute
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "IfElse: ";
        Sym<T>::Print(os);
      }
    };
    
    /** Absolute value expression. */
    template <typename T>
    class AbsSym : public Sym<T>
    {
    public:
      using Sym<T>::Sym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        st.assign(this->index, level, st(this->children[0], level) >= 0 ? st(this->children[0], level) : -st(this->children[0], level));
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Abs: ";
        Sym<T>::Print(os);
      }
    };
    
    // TODO check against CVarArrayExpression
    /** A subexpression dealing with an array **/
    template <typename T>
    class ArraySubSym : public Sym<T>
    {
    public:
      using Sym<T>::Sym;
    };

    /** Minimum of a variable array. */
    template <typename T>
    class MinSym : public ArraySubSym<T>
    {
    public:
      using ArraySubSym<T>::ArraySubSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<VarArraySym<T>>& array = std::dynamic_pointer_cast<VarArraySym<T>>(this->exp_store[this->children[0]]);
        T min = st(array->start, level);
        for (size_t i = 1; i < array->size; i++)
          min = std::min(min, st(array->start + i, level));
        st.assign(this->index, level, min);
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::set<size_t>& changed = st.changed_children(this->index, level);
        T current_min = st(this->index);
        std::set<size_t>::const_iterator min_it = std::min_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
        T changed_min = st(*min_it, level);
        if (changed_min <= current_min)
          st.assign(this->index, level, changed_min);
        else
          this->compute(st, level);
        changed.clear();
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Min: ";
        Sym<T>::Print(os);
      }
    };

    /** Index of the minimum of a variable array. */
    template <typename T>
    class ArgMinSym : public ArraySubSym<T>
    {
    public:
      using ArraySubSym<T>::ArraySubSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<VarArraySym<T>>& array = std::dynamic_pointer_cast<VarArraySym<T>>(this->exp_store[this->children[0]]);
        size_t min_index = 0;
        T min = st(array->start, level);
        for (size_t i = 1; i < array->size; i++)
        {
          T val = st(array->start + i, level);
          if (val < min)
          {
            min = val;
            min_index = i;
          }
        }
        st.assign(this->index, level, min_index);
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::set<size_t>& changed = st.changed_children(this->index, level);
        T current_min = st(this->index);
        std::set<size_t>::const_iterator min_it = std::min_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
        T changed_min = st(*min_it, level);
        if (changed_min <= current_min)
        {
          const std::shared_ptr<VarArraySym<T>>& array = std::dynamic_pointer_cast<VarArraySym<T>>(this->exp_store[this->children[0]]);
          st.assign(this->index, level, *min_it - array->start);
        }
        else
          this->compute(st, level);
        changed.clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "ArgMin: ";
        Sym<T>::Print(os);
      }
    };

    /** Maximum of a variable array. */
    template <typename T>
    class MaxSym : public ArraySubSym<T>
    {
    public:
      using ArraySubSym<T>::ArraySubSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<VarArraySym<T>>& array = std::dynamic_pointer_cast<VarArraySym<T>>(this->exp_store[this->children[0]]);
        T max = st(array->start, level);
        for (size_t i = 1; i < array->size; i++)
          max = std::max(max, st(array->start + i, level));
        st.assign(this->index, level, max);
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::set<size_t>& changed = st.changed_children(this->index, level);
        T current_max = st(this->index);
        std::set<size_t>::const_iterator max_it = std::max_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
        T changed_max = st(*max_it, level);
        if (changed_max >= current_max)
          st.assign(this->index, level, changed_max);
        else
          this->compute(st, level);
        changed.clear();
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Max: ";
        Sym<T>::Print(os);
      }
    };

    /** Index of the max of a variable array. */
    template <typename T>
    class ArgMaxSym : public ArraySubSym<T>
    {
    public:
      using ArraySubSym<T>::ArraySubSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<VarArraySym<T>>& array = std::dynamic_pointer_cast<VarArraySym<T>>(this->exp_store[this->children[0]]);
        size_t max_index = 0;
        T max = st(array->start, level);
        for (size_t i = 1; i < array->size; i++)
        {
          T val = st(array->start + i, level);
          if (val > max)
          {
            max = val;
            max_index = i;
          }
        }
        st.assign(this->index, level, max_index);
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::set<size_t>& changed = st.changed_children(this->index, level);
        T current_max = st(this->index);
        std::set<size_t>::const_iterator max_it = std::max_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
        T changed_max = st(*max_it, level);
        if (changed_max >= current_max)
        {
          const std::shared_ptr<VarArraySym<T>>& array = std::dynamic_pointer_cast<VarArraySym<T>>(this->exp_store[this->children[0]]);
          st.assign(this->index, level, *max_it - array->start);
        }
        else
          this->compute(st, level);
        changed.clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "ArgMax: ";
        Sym<T>::Print(os);
      }
    };

    /** Element expression (element of an array, whose index is a variable). */
    template <typename T>
    class ElementSym : public ArraySubSym<T>
    {
    public:
      using ArraySubSym<T>::ArraySubSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<VarArraySym<T>>& array = std::dynamic_pointer_cast<VarArraySym<T>>(this->exp_store[this->children[0]]);
        size_t offset = st(this->children[1], level);
        st.assign(this->index, level, st(array->start + offset, level));
      }
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Element: ";
        Sym<T>::Print(os);
      }
    };

    /** Generic binary relation expression */
    template <typename T>
    class RelSym : public Sym<T>
    {
    public:
      using Sym<T>::Sym;
      
      /** @copydoc Sym<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
    };

    /** Equality relation. */
    template <typename T>
    class EqSym : public RelSym<T>
    {
    public:
      using RelSym<T>::RelSym;
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Eq: ";
        Sym<T>::Print(os);
      }
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) == st(this->children[1], level));
        st.assign(this->index, level, value);
      }
    };

    /** Inequality relation. */
    template <typename T>
    class NeSym : public RelSym<T>
    {
    public:
      using RelSym<T>::RelSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) != st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Ne: ";
        Sym<T>::Print(os);
      }
    };

    /** Less than relation. */
    template <typename T>
    class LtSym : public RelSym<T>
    {
    public:
      using RelSym<T>::RelSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) < st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Lt: ";
        Sym<T>::Print(os);
      }
    };

    /** Less or equal relation. */
    template <typename T>
    class LeSym : public RelSym<T>
    {
    public:
      using RelSym<T>::RelSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) <= st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Le: ";
        Sym<T>::Print(os);
      }
    };

    /** Greater or equal relation. */
    template <typename T>
    class GeSym : public RelSym<T>
    {
    public:
      using RelSym<T>::RelSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) >= st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Ge: ";
        Sym<T>::Print(os);
      }
    };

    /** Greater than relation. */
    template <typename T>
    class GtSym : public RelSym<T>
    {
    public:
      using RelSym<T>::RelSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) > st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Gt: ";
        Sym<T>::Print(os);
      }
    };
    
    /** Alldifferent relation. */
    template <typename T>
    class AllDiffSym : public RelSym<T>
    {
    public:
      using RelSym<T>::RelSym;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        for (auto it = this->children.begin(); it + 1 != this->children.end(); ++it)
          for (auto jt = it + 1; jt != this->children.end(); jt++)
            if (st(*it, level) == st(*jt, level))
            {
              st.assign(this->index, level, false);
              return;
            }
        st.assign(this->index, level, true);
      }
    
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "AllDiff: ";
        Sym<T>::Print(os);
      }
    };        
  }
}

#endif
