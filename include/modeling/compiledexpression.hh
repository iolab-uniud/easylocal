#pragma once

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

    template <typename T>
    class Exp;

    /** Template compiled Exp<T>. */
    template <typename T>
    struct CExp : public EasyLocal::Core::Printable
    {
    public:

      /** Constructor.
          @param exp_store ExpressionStore into which to register the compiled expression
          @param sym symbol of the compiled expression (for printing purposes)
       */
      CExp(ExpressionStore<T>& es, const std::string& sym) : sym(sym), es(es)
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
      const std::string sym;
      
      /** Reference to the expression store onto which this expression was compiled */
      ExpressionStore<T>& es;
        
      /** Depth of the expression in the AST */
      unsigned int depth;

      /** Computes the value of the expression within ExpressionStore<T> (from scratch).
          @param level level to use for the evaluation
       */
      virtual void compute(unsigned int level = 0) const = 0;

      /** Computes the value of the expression within ExpressionStore<T> (delta from previous value).
          @param level level to use for the evaluation
       */
      virtual void compute_diff(unsigned int level = 0) const = 0;

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        os << this->sym << " ";
        os << "id: " << index << " " << " par: {";
        bool first = true;
        for (auto p : parents)
        {
          if (first)
            first = false;
          else
            os << ", ";
          os << p;
        }
        os << "}, chi: {";
        first = true;
        for (auto p : children)
        {
          if (first)
            first = false;
          else
            os << ", ";
          os << p;
        }
        os << "} orig: " << *exp << " [depth: " << depth << "]";
      }
    };

    /** Terminal symbols. */
    template <typename T>
    class CTerm : public CExp<T> {
    public:
      CTerm(ExpressionStore<T>& es, const std::string& sym) : CExp<T>(es, sym) { }
    };

    /** Scalar variable expression. */
    template <typename T>
    class CVar : public CTerm<T>
    {
    public:

      /** Default constructor. */
      CVar(ExpressionStore<T>& es) : CTerm<T>(es, "CVar") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const { }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const { }
    };

    /** Array variable expression. */
    template <typename T>
    class CArray : public CExp<T>
    {
    public:
      CArray(ExpressionStore<T>& es) : CExp<T>(es, "CArray") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const { }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const { }

      /** Size of the variable array (relies on contiguous allocation of array elements) */
      size_t size;
    };

    /** Numeric constant expression. */
    template <typename T>
    class CConst : public CTerm<T>
    {
    public:
      CConst(ExpressionStore<T>& exp_store) : CTerm<T>(exp_store, "CConst") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        // Note: at first, in the value store the constant value has not been set, so we have to force it
        this->es.set(this->index, level, value);
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const
      {
        // Constant can't be reassigned, therefore there is no need to compute an update
      }

      /** Value of the constant */
      T value;
    };

    /** Summation expression. */
    template <typename T>
    class CSum : public CExp<T>
    {
    public:

      /** Default constructor. */
      CSum(ExpressionStore<T>& es) : CExp<T>(es, "CSum") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      virtual void compute(unsigned int level = 0) const
      {
        T sum = static_cast<T>(0);

        // Sum all children at the same level
        for (size_t child : this->children)
          sum += this->es.get(child, level);

        this->es.set(this->index, level, sum);
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      virtual void compute_diff(unsigned int level = 0) const
      {
        // Get old value
        T value = this->es.get(this->index);

        // Get changed children
        std::unordered_set<size_t>& changed = this->es.changed_children(this->index, level);

        // Iterate through changed children, replace old contribution with new one
        for (auto child : changed)
          value += this->es.get(child, level) - this->es.get(child);

        // Clear list of changed children
        changed.clear();
        this->es.set(this->index, level, value);
      }
    };

    /** Product expression. */
    template <typename T>
    class CMul : public CExp<T>
    {
    public:

      /** Default constructor. */
      CMul(ExpressionStore<T>& es) : CExp<T>(es, "CMul") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      virtual void compute(unsigned int level = 0) const
      {
        T mul = static_cast<T>(1);

        // Multiply the children at the same level
        for (size_t child : this->children)
        {
          // If value is zero, stop updating
          T value = this->es.get(child, level);
          if (value == static_cast<T>(0))
          {
            mul = static_cast<T>(0);
            break;
          }
          mul *= value;
        }
        this->es.set(this->index, level, mul);
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      virtual void compute_diff(unsigned int level = 0) const
      {
        // Get old value
        T value = this->es.get(this->index);

        // Get changed children
        std::unordered_set<size_t>& changed = this->es.changed_children(this->index, level);

        // If any of the children are zero, the product is zero and we're over
        if (std::any_of(begin(changed), end(changed), [this, &level](const size_t& i)->bool { return this->es.get(i, level) == static_cast<T>(0); }))
        {
          this->es.set(this->index, level, static_cast<T>(0));
          changed.clear();
          return;
        }

        // If the previous value was zero, we need to recompute from scratch
        if (value == 0) {
          changed.clear();
          compute(level);
          return;
        }

        /** Compute new contribution, update old contribution. Note that this is
            clean, as whatever we "divide" from the old value, was previously
            multiplied, so te precision is preserved.
         */
        for (auto child : changed)
        {
          // Separated to avoid losing information
          value /= this->es.get(child);
          value *= this->es.get(child, level);
        }

        changed.clear();
        this->es.set(this->index, level, value);
      }
    };

    /** Division expression. */
    template <typename T>
    class CDiv : public CExp<T>
    {
    public:

      /** Default constructor. */
      CDiv(ExpressionStore<T>& es) : CExp<T>(es, "CDiv") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        T res = this->es.get(this->children[0], level) / this->es.get(this->children[1], level);
        this->es.set(this->index, level, res);
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const
      {
        // Since / is binary, it is faster to compute it straight away
        compute(level);
        this->es.changed_children(this->index, level).clear();
      }
    };

    /** Modulo expression. */
    template <typename T>
    class CMod : public CExp<T>
    {
    public:
      CMod(ExpressionStore<T>& es) : CExp<T>(es, "CMod") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        // Since / is binary, it is faster to compute it straight away
        T res = this->es.get(this->children[0], level) % this->es.get(this->children[1], level);
        this->es.set(this->index, level, res);
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const
      {
        compute(level);
        this->es.changed_children(this->index, level).clear();
      }
    };

    /** Minimum expression. */
    template <typename T>
    class CMin : public CExp<T>
    {
    public:

      /** Default constructor. */
      CMin(ExpressionStore<T>& es) : CExp<T>(es, "CMin") { }

      /** @copydoc Sym<T>::compute(unsigned int) */
      virtual void compute(unsigned int level = 0) const
      {
        T min = this->es.get(this->children[0], level);
        for (size_t i = 1; i < this->children.size(); i++)
        {
          size_t child = this->children[i];
          min = std::min(min, this->es.get(child, level));
        }
        this->es.set(this->index, level, min);
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      virtual void compute_diff(unsigned int level = 0) const
      {
        // Get previous value
        T current_min = this->es.get(this->index), new_min;

        // Iterate through changed children, identify min
        bool new_min_set = false;
        for (auto child : this->es.changed_children(this->index, level))
        {
          if (!new_min_set)
          {
            new_min = this->es.get(child, level);
            new_min_set = true;
          }
          else
            new_min = std::min(new_min, this->es.get(child, level));
        }
        this->es.changed_children(this->index, level).clear();

        // If the min among the changed children is worse than the previous min, check all children (the previous min might have been changed)
        if (new_min > current_min)
          for (size_t child : this->children)
            new_min = std::min(new_min, this->es.get(child, level));

        this->es.set(this->index, level, new_min);
      }
    };

    /** Maximum expression. */
    template <typename T>
    class CMax : public CExp<T>
    {
    public:
      CMax(ExpressionStore<T>& exp_store) : CExp<T>(exp_store, "CMax") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      virtual void compute(unsigned int level = 0) const
      {
        T max = this->es.get(this->children[0], level);
        for (size_t i = 1; i < this->children.size(); i++)
        {
          size_t child = this->children[i];
          max = std::max(max, this->es.get(child, level));
        }
        this->es.set(this->index, level, max);
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      virtual void compute_diff(unsigned int level = 0) const
      {
        // Get previous value
        T current_max = this->es.get(this->index), new_max;

        // Iterate through changed children, identify max
        bool new_max_set = false;
        for (auto child : this->es.changed_children(this->index, level))
        {
          if (!new_max_set)
          {
            new_max = this->es.get(child, level);
            new_max_set = true;
          }
          else
            new_max = std::max(new_max, this->es.get(child, level));
        }
        this->es.changed_children(this->index, level).clear();

        // If the new max is worse than the previous, reconsider all the children (previous max might have changed)
        if (new_max < current_max)
          for (size_t child : this->children)
            new_max = std::max(new_max, this->es.get(child, level));
        this->es.set(this->index, level, new_max);
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
      CElement(ExpressionStore<T>& exp_store) : CExp<T>(exp_store, "CElement") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        size_t i = (size_t)(this->es.get(this->children[0], level));

        if (i < 0 || i >= this->exp_store[this->children[1]]->children.size())
          throw std::runtime_error("Index " + std::to_string(i) + "invalid for expression " + std::to_string(this->es[this->children[1]]));

        T e = this->es[this->children[1]]->children[i];
        this->es.set(this->index, level, this->es.get(e, level));
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const
      {
        compute(level);
        this->es.changed_children(this->index, level).clear();
      }
    };

    /** If-then-else expression. */
    template <typename T>
    class CIfThenElse : public CExp<T>
    {
    public:
      CIfThenElse(ExpressionStore<T>& exp_store) : CExp<T>(exp_store, "IfThenElse") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        if (this->es.get(this->children[0], level))
          this->es.set(this->index, level, this->es.get(this->children[1], level));
        else
          this->es.set(this->index, level, this->es.get(this->children[2], level));
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const
      {
        // it is more efficient to directly invoke compute
        compute(level);
        this->es.changed_children(this->index, level).clear();
      }
    };

    /** Absolute value expression. */
    template <typename T>
    class CAbs : public CExp<T>
    {
    public:
      CAbs(ExpressionStore<T>& exp_store) : CExp<T>(exp_store, "CAbs") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        this->es.set(this->index, level, this->es.get(this->children[0], level) >= 0 ? this->es.get(this->children[0], level) : -this->es.get(this->children[0], level));
      }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const
      {
        compute(level);
        this->es.changed_children(this->index, level).clear();
      }
    };

    /** Generic binary relation expression */
    template <typename T>
    class CNoDelta : public CExp<T>
    {
    public:
      CNoDelta(ExpressionStore<T>& exp_store, const std::string& sym) : CExp<T>(exp_store, sym) { }

      /** @copydoc CExp<T>::compute_diff(unsigned int) */
      inline virtual void compute_diff(unsigned int level = 0) const
      {
        this->compute(level);
        this->es.changed_children(this->index, level).clear();
      }
    };

    /** Equality relation. */
    template <typename T>
    class CEq : public CNoDelta<T>
    {
    public:
      CEq(ExpressionStore<T>& exp_store) : CNoDelta<T>(exp_store, "CEq") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        bool value = this->es.get(this->children[0], level) == this->es.get(this->children[1], level);
        this->es.set(this->index, level, value);
      }
    };

    /** Inequality relation. */
    template <typename T>
    class CNe : public CNoDelta<T>
    {
    public:
      CNe(ExpressionStore<T>& exp_store) : CNoDelta<T>(exp_store, "CNe") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        bool value = (this->es.get(this->children[0], level) != this->es.get(this->children[1], level));
        this->es.set(this->index, level, value);
      }
    };

    /** Less than relation. */
    template <typename T>
    class CLt : public CNoDelta<T>
    {
    public:
      CLt(ExpressionStore<T>& exp_store) : CNoDelta<T>(exp_store, "CLt") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        bool value = (this->es.get(this->children[0], level) < this->es.get(this->children[1], level));
        this->es.set(this->index, level, value);
      }
    };

    /** Less or equal relation. */
    template <typename T>
    class CLe : public CNoDelta<T>
    {
    public:
      CLe(ExpressionStore<T>& exp_store) : CNoDelta<T>(exp_store, "CLe") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        bool value = (this->es.get(this->children[0], level) <= this->es.get(this->children[1], level));
        this->es.set(this->index, level, value);
      }
    };

    /** Greater or equal relation. */
    template <typename T>
    class CGe : public CNoDelta<T>
    {
    public:
      CGe(ExpressionStore<T>& exp_store) : CNoDelta<T>(exp_store, "CGe") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        bool value = (this->es.get(this->children[0], level) >= this->es.get(this->children[1], level));
        this->es.set(this->index, level, value);
      }
    };

    /** Greater than relation. */
    template <typename T>
    class CGt : public CNoDelta<T>
    {
    public:
      CGt(ExpressionStore<T>& exp_store) : CNoDelta<T>(exp_store, "CGt") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      inline virtual void compute(unsigned int level = 0) const
      {
        bool value = (this->es.get(this->children[0], level) > this->es.get(this->children[1], level));
        this->es.set(this->index, level, value);
      }
    };

    /** Alldifferent relation. */
    template <typename T>
    class CNValues : public CNoDelta<T>
    {
    public:
      CNValues(ExpressionStore<T>& exp_store) : CNoDelta<T>(exp_store, "CNValues") { }

      /** @copydoc CExp<T>::compute(unsigned int) */
      virtual void compute(unsigned int level = 0) const
      {
        std::unordered_set<T> values;
        for (auto it = begin(this->children); it != end(this->children); it++)
          values.insert(this->es.get(*it, level));
        this->es.set(this->index, level, values.size());
      }
    };
  }
}

