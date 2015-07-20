#ifndef _VALUESTORE_HH
#define _VALUESTORE_HH

#include <sstream>
#include "expressionstore.hh"
#include <stdexcept>

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
        its size to accomodate for changes in its size. This is done to avoid
        imposing a specific initialization order. Ideally, the ValueStore should
        be created after all the expressions have been compiled (so no subscription
        is needed).
     */
    template <typename T>
    class ValueStore : public ResizeSubscriber, public Core::Printable
    {
      friend class ExpressionStore<T>;

    public:
      /** Constructor.
          @param e the ExpressionStore<T> to subscribe to (for resizing)
          @param levels how many levels are supported by this ValueStore<T> (concurrent evaluations)
       */
      ValueStore(std::shared_ptr<ExpressionStore<T>>& es, const size_t& levels) :
        _value(levels + 1, std::vector<T>(es->size())),
        _valid(levels + 1, std::vector<bool>(es->size(), false)),
        _changed_children(levels + 1, std::vector<std::unordered_set<size_t>>(es->size())),
        _es(es),
        _evaluated(false)
      {
        // _valid is initialized to false for each level, except level zero
        std::fill(_valid[0].begin(), _valid[0].end(), true);
      }

      /** Copy constructor (avoids copy of levels above 0).
          @param other ValueStore to get data from
       */
      ValueStore(const ValueStore& other) :
        _value(other.levels() + 1, std::vector<T>(other.size())),
        _valid(other.levels() + 1, std::vector<bool>(other.size(), false)),
        _changed_children(other.size(), std::vector<std::unordered_set<size_t>>(other.size())),
        _es(other._es),
        _evaluated(other._evaluated)
      {
        std::copy(other._value[0].begin(), other._value[0].end(), _value[0].begin());
        std::fill(_valid[0].begin(), _valid[0].end(), true);
        _es->subscribe(this);
      }

      inline size_t size() const
      {
        return _es->size();
      }

      ValueStore(ValueStore<T>&& other) : ValueStore<T>(other._es, other.levels() + 1)
      {
        swap(*this, other);
        _es->subscribe(this);
      }

      ValueStore<T>& operator=(ValueStore<T> other) // (1)
      {
        swap(*this, other); // (2)
        _es->subscribe(this);
        return *this;
      }

      inline friend void swap(ValueStore<T>& first, ValueStore<T>& second) // nothrow
      {
        using std::swap;
        swap(first._value, second._value);
        swap(first._valid, second._valid);
        swap(first._changed_children, second._changed_children);
      }

      /** Gets called by the subscribed ExpressionStore when a resize event is fired.
          @param new_size new size of the ExpressionStore
       */
      void notify(const std::shared_ptr<const ResizeNotifier>& notifier)
      {
        size_t new_size = notifier->size();
        for (size_t l = 0; l < levels() + 1; l++)
        {
          _value[l].resize(new_size);
          _valid[l].resize(new_size, (l == 0));  // initialize to valid if levels is first level
          _changed_children[l].resize(new_size);
        }
        _evaluated = false; // after resize the expression is not evaluated anymore
      }

      /** Resets a specific level of the ValueStore.
          @param level the level to reset
          @remarks _changed_children is not updated, since it is filled and emptied
                  during the bottom-up diff evaluation (invariant: should be always
                  empty before and after diff evaluations).
       */
      inline void reset(unsigned int level)
      {
        std::fill(_valid[level].begin(), _valid[level].end(), false);
        std::fill(_value[level].begin(), _value[level].end(), 0);
      }

      /** Simulates the execution of a simple Change on a specific simulation level.
          @param m the Change to simulate
          @param level level onto which the Change must be simulated
       */
      inline void simulate(const BasicChange<T>& m, unsigned int level = 1)
      {
        if (level > 0)
          reset(level);
        
        if (!_evaluated)
          _es->evaluate(*this);
        
        // Assign tentative value to the var
        size_t var_index = _es->index_of(m.var);
        assign(var_index, level, m.val);
        
        // Create set of vars for evaluation
        std::unordered_set<size_t> vars;
        vars.insert(var_index);
        
        _es.evaluate_diff(*this, vars, level);
      }

      /** Simulates the execution of a composite Change on a specific simulation level.
          @param m the Change to simulate
          @param level level onto which the Change must be simulated
       */
      inline void simulate(const CompositeChange<T>& cm, unsigned int level = 1)
      {
        if (level > 0)
          reset(level);
        
        if (!_evaluated)
          _es->evaluate(*this);

        // Create set of vars for evaluation
        std::unordered_set<size_t> vars;
        for (const BasicChange<T>& m : cm)
        {
          size_t var_index = _es->index_of(m.var);
          assign(var_index, level, m.val);
          vars.insert(var_index);
        }
        _es->evaluate_diff(*this, vars, level);
      }

      /** Executes a simple Change.
          @param m the Change to execute
       */
      inline void execute(const BasicChange<T>& m)
      {
        simulate(m, 1);
        for (size_t i = 0; i < size(); i++)
          if (changed(i, 1))
            assign(i, 0, value(i, 1));
      }

      /** Executes a composite Change.
          @param m the Change to execute
          @remarks it performs the Change on level 1 and then swaps level 1 with level 0
          this means that it cannot be executed in parallel
       */
      inline void execute(const CompositeChange<T>& cm)
      {
        simulate(cm, 1);
        for (size_t i = 0; i < size(); i++)
          if (changed(i, 1))
            assign(i, 0, value(i, 1));
      }

      /** Write access to the values of the expressions in this ValueStore
          @param i the index of the expression to get the value for
          @return the value of the ith expression in this ValueStore
          @remarks write access is only allowed on level zero (simulation levels,
                  i.e., above zero, are only written during Change simulation)
          @return a reference to the value
       */
      inline const T& operator()(const size_t i) const
      {
        return value(i, 0);
      }

      /** Const access to the values of the expressions in the ValueStore
          @param i the index of the expression to get the value for
          @param level level to get the value from
          @return a const reference to the value
       */
      inline const T& operator()(const size_t i, unsigned int level = 0) const
      {
        if (valid(i, level))
          return value(i, level);
        return value(i, 0);
      }

      /** Checks whether the value of an expression at a specific level has changed
       @param i index of the expression to check
       @param level level to check
       */
      inline bool changed(const size_t i, unsigned int level = 0) const
      {
        return valid(i, level) && value(i, level) != value(i, 0);
      }

      /** Write access to the values of the expressions in this ValueStore
          @param e expression to get the value for
          @remarks the expression itself is used to access the ValueStore, instead of the index
          @return a reference to the value
       */
      inline const T& operator()(std::shared_ptr<Exp<T>>& ex)
      {
        // Get index of compiled expression, then value for level zero
        auto i = _es->index_of(ex);
        return operator()(i, 0);
      }

      /** Const access to the values of the expressions in the ValueStore
          @param e expression to get the value for
          @param level level to get the value from
          @remarks the expression itself is used to access the ValueStore, instead of the index
          @return a const reference to the value
       */
      inline const T& operator()(std::shared_ptr<Exp<T>>& ex, unsigned int level = 0) const
      {
        // Get index of compiled expression, then value for requested level
        auto i = _es->index_of(ex);
        return operator()(i, level);
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        for (size_t i = 0; i < size(); i++)
        {
          (*_es)[i]->Print(os);
          os << " (current: " << value(i, 0) << ", values: ";
          for (unsigned int k = 1; k < levels() + 1; k++)
          {
            os << value(i, k) << "/" << std::boolalpha << valid(i,k);
            if (k < levels())
              os << " ";
          }
          os << ")" << std::endl;
        }
      }

      inline void assign(std::shared_ptr<Exp<T>>& ex, unsigned int level, const T& val)
      {
        auto i = _es->index_of(ex);
        assign(i, level, val);
      }

      inline void assign(size_t i, unsigned int level, const T& val)
      {
        _value[level][i] = val;
        _valid[level][i] = true;
      }

      /** Gets the indices of the changed children of an expression at a specific level
          @param i index of the expression to get the changed children for
          @param level reference level
      */
      inline std::unordered_set<size_t>& changed_children(size_t i, unsigned int level)
      {
        return _changed_children[level][i];
      }

      /** Gets the indices of the changed children of an expression at a specific level (const access)
          @param i index of the expression to get the changed children for
          @param level reference level
       */
      inline const std::unordered_set<size_t>& changed_children(const size_t& i, const unsigned int& level) const
      {
        return _changed_children[level][i];
      }
        
      inline size_t levels() const
      {
        return _value.size() - 1;
      }
      
      inline bool& evaluated() const
      {
        return _evaluated;
      }

    protected:

      /** Keeps track of the values of the expressions at the various scenario
          levels. The idea is that level zero represents the current solution,
          the evaluation process works by copying the content of level zero to
          one of the other available levels, and then run a bottom-up re-
          evaluation starting from the changed symbols (variables).
      */
      std::vector<std::vector<T>> _value;

      /** Keeps track whether the value at a specific level is "valid" or
          whether the accessors should fall back to the level zero.
       */
      std::vector<std::vector<bool>> _valid;

      /** Keeps track of the changed children of each expression at each level. */
      std::vector<std::vector<std::unordered_set<size_t>>> _changed_children;

      /** ExpressionStore<T> to which the ValueStore<T> is subscribed to. */
      std::shared_ptr<ExpressionStore<T>> _es;

      /** Whether the first full evaluation has been already run. */
      mutable bool _evaluated;
      
      inline bool valid(size_t i, size_t l) const
      {
        return _valid[l][i];
      }
      
      inline const T& value(size_t i, size_t l) const
      {
        return _value[l][i];
      }
    };
  }
}

#endif
