#ifndef _CEXPRESSION_HH
#define _CEXPRESSION_HH

#include <sstream>
#include <cmath>
#include <memory>
#include <algorithm>
#include <unordered_set>

namespace EasyLocal {
  
  namespace Modeling {
    
    /** Forward declaration */
    template <typename T>
    class ExpressionStore;
    
    /** Forward declaration */
    template <typename T>
    class ValueStore;
    
    /** Template compiled Exp<T>. */
    template <typename T>
    class CExp : public EasyLocal::Core::Printable
    {
    public:
      
      /** Constructor. 
          @param exp_store ExpressionStore into which to register the compiled expression
       */
      CExp(ExpressionStore<T>& exp_store) : exp_store(exp_store)
      {
        depth = 0;
      }
        
      /** Index of the expression in the ExpressionStore */
      size_t index;
      
      /** Parents of the expression in the AST (if any).
          @remarks parents don't need need to be ordered, therefore a unordered_set is appropriate. */
      std::unordered_set<size_t> parents;
      
      /** Children of the expression in the AST (if any).
          @remarks these need to be ordered.
       */
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
        @remarks Currently variables, constants, and arrays
     */
    template <typename T>
    class CTerm : public CExp<T>
    {
    public:
      
      /** Default constructor. */
      using CExp<T>::CExp;
    };

    /** Scalar variable expression. */
    template <typename T>
    class CVar : public CTerm<T>
    {
    public:
     
      /** Default constructor. */
      using CTerm<T>::CTerm;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const { }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const { }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "CVar: ";
        CExp<T>::Print(os);
      }
    };
    
    /** Array variable expression. */
    template <typename T>
    class CArray : public CTerm<T>
    {
    public:
      using CTerm<T>::CTerm;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const { }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const { }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "CArray: ";
        CExp<T>::Print(os);
      }
      
      /** Size of the variable array (relies on contiguous allocation of array elements) */
      size_t size;
    };

    /** Numeric constant expression. */
    template <typename T>
    class CConst : public CTerm<T>
    {
    public:
      using CTerm<T>::CTerm;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        // Note: at first, in the value store the constant value has not been set, so we have to force it
        st.assign(this->index, 0, value);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const { }
      
      /** Value of the constant */
      T value;
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "CConst: ";
        CExp<T>::Print(os);
      }
    };

    /** Summation expression. */
    template <typename T>
    class CSum : public CExp<T>
    {
    public:
      
      /** Default constructor. */
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        T sum = static_cast<T>(0);
        
        // Sum all children at the same level
        for (size_t child : this->children)
          sum += st(child, level);
        
        st.assign(this->index, level, sum);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        // Get old value
        T value = st(this->index);
        
        // Get changed children
        std::unordered_set<size_t>& changed = st.changed_children(this->index, level);
        
        // Iterate through *changed* children, replace old contribution with new one
        for (auto child : changed)
          value += st(child, level) - st(child);
        
        // Clear list of changed children
        changed.clear();
        st.assign(this->index, level, value);
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Sum: ";
        CExp<T>::Print(os);
      }
    };

    /** Product expression. */
    template <typename T>
    class CMul : public CExp<T>
    {
    public:
      
      /** Default constructor. */
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        T mul = static_cast<T>(1);
        
        // Multiply the children at the same level
        for (size_t child : this->children)
        {
          // If value is zero, stop updating
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
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        // Get old value
        T value = st(this->index);
        
        // Get changed children
        std::unordered_set<size_t>& changed = st.changed_children(this->index, level);
        
        // If any of the children are zero, the product is zero and we're over
        if (std::any_of(changed.begin(), changed.end(), [&st,&level](const size_t& i)->bool { return st(i, level) == static_cast<T>(0); }))
        {
          st.assign(this->index, level, static_cast<T>(0));
          changed.clear();
          return;
        }
        
        // If the previous value was zero, we need to recompute from scratch
        if (value == 0) {
          changed.clear();
          return compute(st, level);
        }
        
        /** Compute new contribution, update old contribution. Note that this is
            clean, as whatever we "divide" from the old value, was previously
            multiplied, so te precision is preserved.
         */
        for (auto child : changed)
        {
          // Separated to avoid losing information
          value /= st(child);
          value *= st(child, level);
        }
        
        changed.clear();
        st.assign(this->index, level, value);
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Mul: ";
        CExp<T>::Print(os);
      }
    };
    
    /** Division expression. */
    template <typename T>
    class CDiv : public CExp<T>
    {
    public:
      
      /** Default constructor. */
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        T res = st(this->children[0], level) / st(this->children[1], level);
        st.assign(this->index, level, res);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        // Since / is binary, it is faster to compute it straight away
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Div: ";
        CExp<T>::Print(os);
      }
    };
    
    /** Modulo expression. */
    template <typename T>
    class CMod : public CExp<T>
    {
    public:
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        // Since / is binary, it is faster to compute it straight away
        T res = st(this->children[0], level) % st(this->children[1], level);
        st.assign(this->index, level, res);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Mod: ";
        CExp<T>::Print(os);
      }
    };
    
    /** Minimum expression. */
    template <typename T>
    class CMin : public CExp<T>
    {
    public:
      
      /** Default constructor. */
      using CExp<T>::CExp;
      
      /** @copydoc Sym<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        T min = st(this->children[0], level);
        for (size_t i = 1; i < this->children.size(); i++)
        {
          size_t child = this->children[i];
          min = std::min(min, st(child, level));
        }
        st.assign(this->index, level, min);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        // TODO: add field for relevant children
        
        // Get previous value
        T current_min = st(this->index), new_min;
        
        // Iterate through changed children, identify min
        bool new_min_set = false;
        for (auto child : st.changed_children(this->index, level))
        {
          if (!new_min_set)
          {
            new_min = st(child, level);
            new_min_set = true;
          }
          else
            new_min = std::min(new_min, st(child, level));
        }
        st.changed_children(this->index, level).clear();
        
        // If the min among the changed children is worse than the previous min, check all children (the previous min might have been changed)
        if (new_min > current_min)
          for (size_t child : this->children)
            new_min = std::min(new_min, st(child, level));
        
        st.assign(this->index, level, new_min);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Min: ";
        CExp<T>::Print(os);
      }
    };
    
    /** Maximum expression. */
    template <typename T>
    class CMax : public CExp<T>
    {
    public:
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        T max = st(this->children[0], level);
        for (size_t i = 1; i < this->children.size(); i++)
        {
          size_t child = this->children[i];
          max = std::max(max, st(child, level));
        }
        st.assign(this->index, level, max);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        // TODO: add field for relevant children
        
        // Get previous value
        T current_max = st(this->index), new_max;
        
        // Iterate through changed children, identify max
        bool new_max_set = false;
        for (auto child : st.changed_children(this->index, level))
        {
          if (!new_max_set)
          {
            new_max = st(child, level);
            new_max_set = true;
          }
          else
            new_max = std::max(new_max, st(child, level));
        }
        st.changed_children(this->index, level).clear();
        
        // If the new max is worse than the previous, reconsider all the children (previous max might have changed)
        if (new_max < current_max)
          for (size_t child : this->children)
            new_max = std::max(new_max, st(child, level));
        st.assign(this->index, level, new_max);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Max: ";
        CExp<T>::Print(os);
      }
    };
    
    /** 
     Element expression (element of a list of variables, whose index is a variable).
     @remark the first child is the index expression, whereas the remaining ones represent the list
     */
    template <typename T>
    class CElement : public CExp<T>
    {
    public:
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        int offset = st(this->children[0], level);
        if (offset < 0 || offset >= this->children.size() - 1)
          throw std::runtime_error("Error: ArrayElement expression using an invalid index (index value: " + std::to_string(offset) + ", list size: " + std::to_string(this->children.size() - 1) + ")");
        st.assign(this->index, level, st(this->children[1 + offset], level));
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Element: ";
        CExp<T>::Print(os);
      }
    };

    /** If-then-else expression. */
    template <typename T>
    class CIfThenElse : public CExp<T>
    {
    public:
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        if (st(this->children[0], level))
          st.assign(this->index, level, st(this->children[1], level));
        else
          st.assign(this->index, level, st(this->children[2], level));
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
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
        CExp<T>::Print(os);
      }
    };
    
    /** Absolute value expression. */
    template <typename T>
    class CAbs : public CExp<T>
    {
    public:
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        st.assign(this->index, level, st(this->children[0], level) >= 0 ? st(this->children[0], level) : -st(this->children[0], level));
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Abs: ";
        CExp<T>::Print(os);
      }
    };
    
    // TODO check against CArrayExpression
    /** A subexpression dealing with an array **/
    template <typename T>
    class CArraySub : public CExp<T>
    {
    public:
      using CExp<T>::CExp;
    };

    /** Minimum of a variable array. */
    template <typename T>
    class CArrayMin : public CArraySub<T>
    {
    public:
      using CArraySub<T>::CArraySub;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<CArray<T>>& array = std::static_pointer_cast<CArray<T>>(this->exp_store[this->children[0]]);
        T min = st(array->index, level);
        for (size_t i = 1; i < array->size; i++)
          min = std::min(min, st(array->index + i, level));
        st.assign(this->index, level, min);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::unordered_set<size_t>& changed = st.changed_children(this->index, level);
        T current_min = st(this->index);
        std::unordered_set<size_t>::const_iterator min_it = std::min_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
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
        os << "ArrayMin: ";
        CExp<T>::Print(os);
      }
    };

    /** Index of the minimum of a variable array. */
    template <typename T>
    class CArgMin : public CArraySub<T>
    {
    public:
      using CArraySub<T>::CArraySub;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<CArray<T>>& array = std::static_pointer_cast<CArray<T>>(this->exp_store[this->children[0]]);
        size_t min_index = 0;
        T min = st(array->index, level);
        for (size_t i = 1; i < array->size; i++)
        {
          T val = st(array->index + i, level);
          if (val < min)
          {
            min = val;
            min_index = i;
          }
        }
        st.assign(this->index, level, min_index);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::unordered_set<size_t>& changed = st.changed_children(this->index, level);
        T current_min = st(this->index);
        std::unordered_set<size_t>::const_iterator min_it = std::min_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
        T changed_min = st(*min_it, level);
        if (changed_min <= current_min)
        {
          const std::shared_ptr<CArray<T>>& array = std::static_pointer_cast<CArray<T>>(this->exp_store[this->children[0]]);
          st.assign(this->index, level, *min_it - array->index);
        }
        else
          this->compute(st, level);
        changed.clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "ArgMin: ";
        CExp<T>::Print(os);
      }
    };

    /** Maximum of a variable array. */
    template <typename T>
    class CArrayMax : public CArraySub<T>
    {
    public:
      using CArraySub<T>::CArraySub;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<CArray<T>>& array = std::static_pointer_cast<CArray<T>>(this->exp_store[this->children[0]]);
        T max = st(array->index, level);
        for (size_t i = 1; i < array->size; i++)
          max = std::max(max, st(array->index + i, level));
        st.assign(this->index, level, max);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::unordered_set<size_t>& changed = st.changed_children(this->index, level);
        T current_max = st(this->index);
        std::unordered_set<size_t>::const_iterator max_it = std::max_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
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
        os << "ArrayMax: ";
        CExp<T>::Print(os);
      }
    };

    /** Index of the max of a variable array. */
    template <typename T>
    class CArgMax : public CArraySub<T>
    {
    public:
      using CArraySub<T>::CArraySub;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<CArray<T>>& array = std::static_pointer_cast<CArray<T>>(this->exp_store[this->children[0]]);
        size_t max_index = 0;
        T max = st(array->index, level);
        for (size_t i = 1; i < array->size; i++)
        {
          T val = st(array->index + i, level);
          if (val > max)
          {
            max = val;
            max_index = i;
          }
        }
        st.assign(this->index, level, max_index);
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        std::unordered_set<size_t>& changed = st.changed_children(this->index, level);
        T current_max = st(this->index);
        std::unordered_set<size_t>::const_iterator max_it = std::max_element(changed.begin(), changed.end(), [&st,&level](const size_t& e1, const size_t& e2)->bool { return st(e1, level) < st(e2, level); });
        T changed_max = st(*max_it, level);
        if (changed_max >= current_max)
        {
          const std::shared_ptr<CArray<T>>& array = std::static_pointer_cast<CArray<T>>(this->exp_store[this->children[0]]);
          st.assign(this->index, level, *max_it - array->index);
        }
        else
          this->compute(st, level);
        changed.clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "ArgMax: ";
        CExp<T>::Print(os);
      }
    };

    /** Element expression (element of an array, whose index is an expression involving variables). 
     @remark the first child is the index expression, the second one is the array
     */
    template <typename T>
    class CArrayElement : public CArraySub<T>
    {
    public:
      using CArraySub<T>::CArraySub;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        const std::shared_ptr<CArray<T>>& array = std::static_pointer_cast<CArray<T>>(this->exp_store[this->children[1]]);
        int offset = st(this->children[0], level);
        if (offset < 0 || offset >= array->size)
          throw std::runtime_error("Error: ArrayElement expression using an invalid index (index value: " + std::to_string(offset) + ")");
        st.assign(this->index, level, st(array->index + offset, level));
      }
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Element: ";
        CExp<T>::Print(os);
      }
    };

    /** Generic binary relation expression */
    template <typename T>
    class CRel : public CExp<T>
    {
    public:
      using CExp<T>::CExp;
      
      /** @copydoc CExp<T>::compute_diff(ValueStore<T>&, unsigned int) */
      virtual void compute_diff(ValueStore<T>& st, unsigned int level = 0) const
      {
        this->compute(st, level);
        st.changed_children(this->index, level).clear();
      }
    };

    /** Equality relation. */
    template <typename T>
    class CEq : public CRel<T>
    {
    public:
      using CRel<T>::CRel;
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Eq: ";
        CExp<T>::Print(os);
      }
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) == st(this->children[1], level));
        st.assign(this->index, level, value);
      }
    };

    /** Inequality relation. */
    template <typename T>
    class CNe : public CRel<T>
    {
    public:
      using CRel<T>::CRel;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) != st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Ne: ";
        CExp<T>::Print(os);
      }
    };

    /** Less than relation. */
    template <typename T>
    class CLt : public CRel<T>
    {
    public:
      using CRel<T>::CRel;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) < st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Lt: ";
        CExp<T>::Print(os);
      }
    };

    /** Less or equal relation. */
    template <typename T>
    class CLe : public CRel<T>
    {
    public:
      using CRel<T>::CRel;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) <= st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Le: ";
        CExp<T>::Print(os);
      }
    };

    /** Greater or equal relation. */
    template <typename T>
    class CGe : public CRel<T>
    {
    public:
      using CRel<T>::CRel;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) >= st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Ge: ";
        CExp<T>::Print(os);
      }
    };

    /** Greater than relation. */
    template <typename T>
    class CGt : public CRel<T>
    {
    public:
      using CRel<T>::CRel;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
      virtual void compute(ValueStore<T>& st, unsigned int level = 0) const
      {
        bool value = (st(this->children[0], level) > st(this->children[1], level));
        st.assign(this->index, level, value);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << "Gt: ";
        CExp<T>::Print(os);
      }
    };
    
    /** Alldifferent relation. */
    template <typename T>
    class CAllDiff : public CRel<T>
    {
    public:
      using CRel<T>::CRel;
      
      /** @copydoc CExp<T>::compute(ValueStore<T>&, unsigned int) */
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
        CExp<T>::Print(os);
      }
    };        
  }
}

#endif
