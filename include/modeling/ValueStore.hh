#if !defined(_VALUESTORE_HH_)
#define _VALUESTORE_HH_

#include <sstream>

#include "modeling/ExpressionStore.hh"

namespace EasyLocal {
    
  namespace Modeling {
    
    /** Forward declaration */
    template <typename T>
    class BasicChange;
    
    /** Forward declaration */
    template <typename T>
    class CompositeChange;
    
    /** Forward declaration */
    template <typename T>
    class Var;
        
    /** A store for the values of CompiledExpressions, used to efficiently compute
    delta changes in the expression values, and to support concurrent simulation
    of Changes. 
    @remarks The ValueStore subscribes to an ExpressionStore, in order to update
    its size to accomodate for changes in the size of the expression.
    */
    template <typename T>
    class ValueStore : public ResizeNotify, public Printable
    {
      friend class ExpressionStore<T>;
    public:
      /** Constructor.
      @param e the expression store to subscribe to (for resizing)
      @param levels how many levels are supported by this ValueStore (concurrent evaluations)
      @remarks the "valid" and "_changed_children" variables are not meaninful for level 0, this allows for a small memory footprint improvement (TODO)
      */
      ValueStore(const std::shared_ptr<ExpressionStore<T>>& e, size_t levels) : value(levels + 1, std::vector<T>(e->size())), valid(levels + 1, std::vector<bool>(e->size(), false)), _changed_children(levels + 1, std::vector<std::set<size_t>>(e->size())), e(e)
      {
        std::fill(valid[0].begin(), valid[0].end(), true);
        e->subscribe(this);
        evaluated = false;
      }
      
      /** Copy constructor (avoids copy of levels above 0).
      @param other ValueStore to get data from
      */
      ValueStore(const ValueStore& other) : value(other.value.size(), std::vector<T>(other.e->size())), valid(other.valid.size(), std::vector<bool>(other.e->size(), false)), _changed_children(other._changed_children.size(), std::vector<std::set<size_t>>(other.e->size())), e(other.e)
      {
        std::copy(other.value[0].begin(), other.value[0].end(), value[0].begin());
        std::fill(valid[0].begin(), valid[0].end(), true);
        e->subscribe(this);
        evaluated = other.evaluated;
      }
    protected:
      ValueStore(const std::shared_ptr<ExpressionStore<T>>& e) : e(e) {}

    public:

      ValueStore(ValueStore<T>&& other) : ValueStore<T>(other.e)
      {
        swap(*this, other);
      }

      ValueStore<T>& operator=(ValueStore<T> other) // (1)
      {
        swap(*this, other); // (2)
        return *this;
      }

      friend void swap(ValueStore<T>& first, ValueStore<T>& second) // nothrow
      {
        using std::swap;
        swap(first.value, second.value);
        swap(first.valid, second.valid);
        swap(first._changed_children, second._changed_children);
      }
      
      /** 
      Gets called by the subscribed ExpressionStore when a resize event is fired.
      @param new_size new size of the ExpressionStore
      */
      void resized(size_t new_size)
      {
        for (size_t l = 0; l < value.size(); l++)
        {
          value[l].resize(new_size);
          valid[l].resize(new_size, (l == 0));
          _changed_children[l].resize(new_size);
        }
        evaluated = false;
      }
      
      /** Resets a specific level of the ValueStore. 
      @param level the level to reset
      @remark _changed_children is not updated, since it is filled and emptied during the bottom-up diff evaluation (invariant: should be always empty before and after diff evaluations)
      */
      void reset(unsigned int level)
      {
        std::fill(valid[level].begin(), valid[level].end(), false);
        std::fill(value[level].begin(), value[level].end(), 0);
      }
      
      /** Simulates the execution of a simple Change on a specific simulation level.
      @param m the Change to simulate
      @param level level onto which the Change must be simulated
      */
      void simulate(const BasicChange<T>& m, unsigned int level = 1)
      {
        if (level > 0)
          reset(level);
        if (!evaluated)
          e->evaluate(*this);
        std::set<size_t> vars;
        assign(m.var, level, m.val);
        size_t var_index = e->compiled_symbols[m.var.hash()];
        vars.insert(var_index);
        e->evaluate_diff(*this, vars, level);
      }
      
      /** Simulates the execution of a composite Change on a specific simulation level.
      @param m the Change to simulate
      @param level level onto which the Change must be simulated
      */
      void simulate(const CompositeChange<T>& cm, unsigned int level = 1)
      {
        if (level > 0)
          reset(level);
        if (!evaluated)
          e->evaluate(*this);
        std::set<size_t> vars;
        for (const BasicChange<T>& m : cm)
        {
          assign(m.var, level, m.val);
          size_t var_index = e->compiled_symbols[m.var.hash()];
          vars.insert(var_index);
        }        
        e->evaluate_diff(*this, vars, level);
      }

      /** Executes a simple Change.
      @param m the Change to execute
      @remarks it performs the Change on level 1 and then swaps level 1 with level 0
      this means that it cannot be executed in parallel
      */
      void execute(const BasicChange<T>& m)
      {
        simulate(m, 1);
        for (size_t i = 0; i < value[0].size(); i++)
          if (this->changed(i, 1))
            value[0][i] = value[1][i];
      }

      /** Executes a composite Change.
      @param m the Change to execute
      @remarks it performs the Change on level 1 and then swaps level 1 with level 0
      this means that it cannot be executed in parallel
      */
      void execute(const CompositeChange<T>& cm)
      {
        simulate(cm, 1);
        for (size_t i = 0; i < value[0].size(); i++)
          if (this->changed(i, 1))
            value[0][i] = value[1][i];
      }
      
      /** Write access to the values of the expressions in this ValueStore
      @param i the index of the expression to get the value for
      @return the value of the ith expression in this ValueStore
      @remarks write access is only allowed on level zero (simulation levels, i.e., above zero, are only written during Change simulation)
      @return a reference to the value
      */
      T& operator()(const size_t i)
      {
        return value[0][i];
      }
      
      /** Const access to the values of the expressions in the ValueStore
      @param i the index of the expression to get the value for
      @param level level to get the value from
      @return a const reference to the value
      */
      const T& operator()(const size_t i, unsigned int level = 0) const
      {
        if (valid[level][i])
          return value[level][i];
        else
          return value[0][i];
      }
      
      /** Checks whether the value of an expression at a specific level has changed 
      @param i index of the expression to check
      @param level level to check
      */
      inline bool changed(const size_t i, unsigned int level = 0) const
      {
        return valid[level][i] && value[level][i] != value[0][i];
      }
      
      /** Write access to the values of the expressions in this ValueStore
      @param e expression to get the value for
      @remarks the expression itself is used to access the ValueStore, instead of the index 
      @return a reference to the value
      */
      T& operator()(const Sym<T>& ex)
      {
        return operator()(ex.index);
      }
      
      /** Const access to the values of the expressions in the ValueStore
      @param e expression to get the value for
      @param level level to get the value from
      @remarks the expression itself is used to access the ValueStore, instead of the index
      @return a const reference to the value
      */
      const T& operator()(const Sym<T>& ex, unsigned int level = 0) const
      {
        return operator()(ex.index, level);
      }
      
      /** Checks whether the value of an expression at a specific level has changed
      @param i index of the expression to check
      @param level level to check
      @remarks the expression itself is used to access the ValueStore, instead of the index
      */
      bool changed(const Sym<T>& ex, unsigned int level = 0) const
      {
        return changed(ex.index, level);
      }
      
      /** Write access to the values of the variables in this ValueStore
      @param v the variable to get the value for
      @return a reference to the value
      */
      T& operator()(const Var<T>& v)
      {
        return operator()(e->compiled_symbols[v.hash()]);
      }
      
      /** Const access to the values of the variables in the ValueStore
      @param v the variable to get the value for
      @param level the level to get the value from
      @return a const reference to the value
      */
      const T& operator()(const Var<T>& v, unsigned int level = 0) const
      {
        return operator()(e->compiled_symbols[v.hash()], level);
      }
      
      /** Checks whether the variable of an expression at a specific level has changed
      @param v the variable to check
      @param level level to check
      */
      bool changed(const Var<T>& v, unsigned int level = 0) const
      {
        return changed(e->compiled_symbols[v.hash()], level);
      }
      
      /** @copydoc Printable::print(std::ostream&) */
      virtual void print(std::ostream& os) const
      {
        for (size_t i = 0; i < e->size(); i++)
        {
          (*e)[i]->print(os);
          os << " (current: " << value[0][i] << ", values: ";
          for (unsigned int k = 1; k < value.size(); k++)
            os << value[k][i] << "/" << valid[k][i] << " ";
          os << ")" << std::endl;
        }
      }
      
      // TODO: all assign are used only by Expressions, possibly Change into protected and enable friendship
      void assign(const Sym<T>& ex, unsigned int level, const T& val)
      {
        value[level][ex.index] = val;
        valid[level][ex.index] = true;
      }
      
      void assign(size_t i, unsigned int level, const T& val)
      {
        value[level][i] = val;
        valid[level][i] = true;
      }
      
      void assign(const Var<T>& v, unsigned int level, const T& val)
      {
        value[level][e->compiled_symbols[v.hash()]] = val;
        valid[level][e->compiled_symbols[v.hash()]] = true;
      }
      
      /** Gets the indices of the changed children of an expression at a specific level
      @param i index of the expression to get the changed children for
      @param level reference level
      */
      std::set<size_t>& changed_children(size_t i, unsigned int level)
      {
        return _changed_children[level][i];
      }
      
      /** Gets the indices of the changed children of an expression at a specific level (const access) 
      @param i index of the expression to get the changed children for
      @param level reference level
      */
      const std::set<size_t>& changed_children(size_t i, unsigned int level) const
      {
        return _changed_children[level][i];
      }

    protected:
      
      /** Keeps track of the values of the expressions / variables at the various levels */
      std::vector<std::vector<T>> value;
      
      /** Keeps track whether the value at a specific level is valid or whether the accessors should fall back to the level zero */
      std::vector<std::vector<bool>> valid;
      
      /** Keeps track of the changed children of each expression */
      std::vector<std::vector<std::set<size_t>>> _changed_children;
      
      /** ExpressionStore to which the ValueStore is subscribed for resizing */
      const std::shared_ptr<ExpressionStore<T>>& e;
      
      /** Whether the first complete evaluation has been already done */
      mutable bool evaluated;
    };
  }
}

#endif // _VALUESTORE_HH_