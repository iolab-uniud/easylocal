#pragma once

#include <queue>
#include <unordered_map>

#include "expression.hh"
#include "change.hh"

namespace EasyLocal {

  namespace Modeling {

    /** Proxy to Exp<T>::hash() */
    template <typename T>
    struct ExpHash {
      inline std::size_t operator()(const std::shared_ptr<const Exp<T>>& key) const
      {
        return key->hash();
      }
    };

    /** Proxy to Exp<T>::equals_to(const std::shared_ptr<Exp<T>>&) */
    template <typename T>
    struct ExpEquals {
      inline bool operator()(const std::shared_ptr<const Exp<T>>& l_key, const std::shared_ptr<const Exp<T>>& r_key) const
      {
        return l_key->equals_to(r_key);
      }
    };

    /** ExpressionStore, a structure to handle bottom-up evaluation of compiled Exps. */
    template <typename T>
    class ExpressionStore : public std::vector<std::shared_ptr<CExp<T>>>, public Core::Printable
    {
      
      friend struct CExp<T>;
      friend class Exp<T>;
      
    public:
      
      /** Constructor.
          @param levels how many levels are supported by this ExpressionStore<T> (concurrent evaluations)
       */
      ExpressionStore(const size_t& levels) : _value(levels + 1), _valid(levels + 1), _changed_children(levels + 1), _evaluated(false), _needs_update(false) { }
      
      /** Copy constructor. 
          @param other ExpressionStore<T> to copy from
       */
      ExpressionStore(const ExpressionStore<T>& other) : std::vector<std::shared_ptr<CExp<T>>>(other),
      _value(other.levels() + 1, std::vector<T>(other.size())),
      _valid(other.levels() + 1, std::vector<bool>(other.size(), false)),
      _changed_children(other.levels(), std::vector<std::unordered_set<size_t>>(other.size())),
      _evaluated(other._evaluated),
      _needs_update(false)
      {
        // Copy values at level zero (maintain incumbent solution)
        std::copy(begin(other._value[0]), end(other._value[0]), begin(_value[0]));
        std::fill(begin(_valid[0]), end(_valid[0]), true);
      }
      
      ExpressionStore<T>& operator=(ExpressionStore<T> other) // (1) passed by value
      {
        swap(*this, other); // (2) use swap to replace
        return *this;
      }
      
      /** Compile an expression into a compiled expression.
       @param e the original expression
       @return a shared pointer to the root of the compiled expression
       */
      inline void compile(std::shared_ptr<Exp<T>>& e)
      {
        // Return if expression is compiled already
        if (this->contains(e))
          return;
        
        // Otherwise: compile and normalize
        e->normalize(true);
        e->compile(*this);
        
        // We need to update the data structures
        _needs_update = true;
      }
      
      /** Evaluates all the registered expressions within a given ValueStore at a given level.
       @param level level on which to evaluate
       */
      void evaluate(unsigned int level = 0, bool force = false) const
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        // Make list of terminal expressions
        std::unordered_set<size_t> to_update;
        for (size_t i = 0; i < this->size(); i++)
        {
          if (std::static_pointer_cast<CVar<T>>((*this)[i]) == nullptr)
            continue;
          to_update.insert(i);
        }
        
        // Call evaluate, passing list of all terminals as list of all terminals to update
        _evaluate(to_update, level, force);
      }
      
      /** Gets the indices of the changed children of an expression at a specific level
       @param i index of the expression to get the changed children for
       @param level reference level
       */
      inline std::unordered_set<size_t>& changed_children(size_t i, unsigned int level)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        return _changed_children[level][i];
      }
      
      /** Resets a specific level of the ValueStore.
       @param level the level to reset
       @remarks _changed_children is not updated, since it is filled and emptied
       during the bottom-up diff evaluation (invariant: should be always
       empty before and after diff evaluations).
       */
      inline void reset(unsigned int level)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        if (level == 0)
          _evaluated = false;
        
        // Invalidate all fields
        std::fill(begin(_valid[level]), end(_valid[level]), false);
      }
      
      /** Simulates the execution of a simple Change on a specific simulation level.
       @param m the Change to simulate
       @param level level onto which the Change must be simulated
       */
      inline void simulate(const BasicChange<T>& c, unsigned int level = 1)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        // Reset level
        if (level > 0)
          reset(level);
        
        // Assign tentative value to the var
        auto v_index = index_of(c.var);
        set(v_index, level, c.val);
        
        // Evaluate
        evaluate_diff(std::unordered_set<size_t>({ v_index }), level);
      }
      
      /** Simulates the execution of a composite Change on a specific simulation level.
       @param m the Change to simulate
       @param level level onto which the Change must be simulated
       */
      inline void simulate(const CompositeChange<T>& cc, unsigned int level = 1)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        // Reset level
        if (level > 0)
          reset(level);
        
        // Create set of vars for evaluation
        std::unordered_set<size_t> vars;
        for (const BasicChange<T>& c : cc)
        {
          size_t v_index = index_of(c.var);
          set(v_index, level, c.val);
          vars.insert(v_index);
        }
        evaluate_diff(vars, level);
      }
      
      /** Executes a simple Change.
       @param m the Change to execute
       @remarks it performs the Change on level 1 and then swaps level 1 with
       level 0 (this is not done with std::swap because there might
       values that are not valid).
       */
      inline void execute(const BasicChange<T>& m)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        // Simulate then
        simulate(m, 1);
        for (size_t i = 0; i < this->size(); i++)
          if (changed(i, 1))
            set(i, 0, value(i, 1));
      }
      
      /** Executes a composite Change.
       @param m the Change to execute
       @remarks it performs the Change on level 1 and then swaps level 1 with
       level 0 (this is not done with std::swap because there might
       values that are not valid).
       */
      inline void execute(const CompositeChange<T>& cm)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        simulate(cm, 1);
        for (size_t i = 0; i < this->size(); i++)
          if (changed(i, 1))
            set(i, 0, value(i, 1));
      }
      
      /** Const access to the values of the expressions in the ValueStore
       @param e expression to get the value for
       @param level level to get the value from
       @remarks the expression itself is used to access the ValueStore, instead of the index
       @return a const reference to the value
       */
      inline const T& get(std::shared_ptr<Exp<T>>& ex, unsigned int level = 0)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        auto ref = std::static_pointer_cast<const Exp<T>>(ex);
        return get(ref, level);
      }
        
      /** Const access to the values of the expressions in the ValueStore
       @param e expression to get the value for
       @param level level to get the value from
       @remarks the expression itself is used to access the ValueStore, instead of the index
       @return a const reference to the value
       */
      inline const T& get(std::shared_ptr<const Exp<T>>& ex, unsigned int level = 0)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        // Get index of compiled expression, then value for requested level
        auto i = index_of(ex);
        return value(i, level);
      }
      
      /** Const access to the values of the expressions in the ValueStore
       @param i index
       @param level level to get the value from
       @remarks the expression itself is used to access the ValueStore, instead of the index
       @return a const reference to the value
       */
      inline const T& get(const size_t& i, unsigned int level = 0)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        // Get index of compiled expression, then value for requested level
        return value(i, level);
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      { 
        for (size_t i = 0; i < this->size(); i++)
        {
          this->at(i)->Print(os);
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
      
      inline void set(std::shared_ptr<Exp<T>>& ex, const T& val, unsigned int level = 0)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        auto ref = std::static_pointer_cast<const Exp<T>>(ex);
        set(index_of(ref), val, level);
      }
      
      inline void set(size_t i, const T& val, unsigned int level = 0)
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        _value[level][i] = val;
        _valid[level][i] = true;
      }
      
    protected:
      
      /** Nothrow swap, for implementing copy constructor. */
      inline friend void swap(ExpressionStore<T>& first, ExpressionStore<T>& second) // (3) nothrow
      {
        using std::swap;
        swap(first._value, second._value);
        swap(first._valid, second._valid);
        swap(first._changed_children, second._changed_children);
        swap(first._evaluated, second._evaluated);
      }
      
      /** Register compiled expression with modeling expression (used by expressions for compilation).
          @param cexp compiled expression
          @param corresponding modeling expression
          @return index of the expression in the ExpressionStore<T>
       */
      inline const size_t register_as(const std::shared_ptr<CExp<T>>& cexp, const std::shared_ptr<const Exp<T>>& exp)
      {
        // If expression is already there, return its index
        auto it = _compiled.find(exp);
        if (it != end(_compiled))
          return it->second;
        
        // Register the hash in the list of compiled expressions
        auto pre_size = this->size();
        _compiled[exp] = pre_size;
        this->push_back(cexp);
        
        // Return index
        return pre_size;
      }
      
      /** Check whether an expression is already in the ExpressionStore<T>
          @param exp Exp<T> to check
       */
      inline bool contains(const std::shared_ptr<const Exp<T>>& exp) const
      {
        return _compiled.find(exp) != end(_compiled);
      }
      
      /** Index of expression in ExpressionStore<T> 
          @param e expression to get the index of
          @remarks assumes that we have ensured that the expression is in the
                   store, throw runtime exception if not.
       */
      inline size_t index_of(std::shared_ptr<const Exp<T>>& e) const
      {
        // Return directly index if expression is compiled already
        auto it = _compiled.find(e);
        if (it != end(_compiled))
          return it->second;
        throw std::runtime_error("Expression is not compiled!");
      }
      
      /** Checks whether the value of an expression at a specific level has changed
       @param i index of the expression to check
       @param level level to check
       */
      inline bool changed(const size_t i, unsigned int level = 0) const
      {
        // Update data structures if needed
        if (_needs_update)
          _update();
        
        if (level == 0)
          return false;
        return valid(i, level) && value(i, level) != value(i, 0);
      }


      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of variables that have been changed (delta).
          @param variables a set of variables that have been changed
          @param level level on which to evaluate
       */
      void evaluate_diff(const std::unordered_set<size_t>& to_update, unsigned int level) const
      {
        // Update depth if needed
        if (_needs_update)
          _update();

        // Initialize empty queue
        std::priority_queue<std::pair<int, size_t>> queue;

        // Set of already processed nodes
        std::unordered_set<size_t> processed;
        processed.reserve(this->size());

        // Go through all modified terminal symbols
        for (auto t : to_update)
        {
          // If terminal hasn't really changed, continue (a move might actually not change a variable, for some reason)
          if (!changed(t, level))
            continue;

          // Add all parents of modified terminals
          const std::shared_ptr<CExp<T>>& cur = this->at(t);
          for (size_t i : cur->parents)
          {
            const std::shared_ptr<CExp<T>>& par = this->at(i);

            // If parent has not been enqueued already
            if (processed.find(i) == end(processed))
            {
              queue.push(std::make_pair(par->depth, i));
              processed.insert(i);
            }
            changed_children(i, level).insert(t);
          }
        }

        // Processe elements until queue is empty
        while (!queue.empty())
        {
          // Dequeue expression
          size_t cur_i = queue.top().second;
          queue.pop();

          // Get compiled expression
          const auto& cur = this->at(cur_i);

          // Compute new value (with diff)
          cur->compute_diff(level);

          // If value has changed, and parent is not enqueued already, enqueue it
          if (changed(cur_i, level))
            for (size_t i : cur->parents)
            {
              const std::shared_ptr<CExp<T>>& par = this->at(i);
              if (processed.find(i) == end(processed))
              {
                queue.push(std::make_pair(par->depth, i));
                processed.insert(i);
              }

              // Update list of changed children for parent
              changed_children(i, level).insert(cur_i);
            }
        }
      }

      /** Cached version of depth update. */
      inline void _update()
      {
        // Update data structures' size
        _update_size();
        
        // Update depth
        for (size_t i = 0; i < this->size(); i++)
          _update_depth(i, 0);
      
        // Mark as updated
        _needs_update = false;
      }
      
      /** Gets called by the subscribed ExpressionStore when a resize event is fired.
       */
      void _update_size()
      {
        for (size_t l = 0; l < levels() + 1; l++)
        {
          _value[l].resize(this->size());
          _valid[l].resize(this->size(), (l == 0));  // initialize to valid if levels is first level
          _changed_children[l].resize(this->size());
        }
        _evaluated = false; // after resize the expression must be re-evaluated
      }
      
      inline size_t levels() const
      {
        return _value.size() - 1;
      }
      
      inline bool& evaluated() const
      {
        return _evaluated;
      }
      
      inline bool valid(size_t i, size_t l) const
      {
        return _valid[l][i];
      }
      
      inline const T& value(size_t i, size_t l) const
      {
        return _value[l][i];
      }

      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of expressions whose value has changed (delta).
          @param expressions a set of expressions that have been changed
          @param level level on which to evaluate
          @remarks Internally used by the public evaluate method
       */
      void _evaluate(const std::unordered_set<size_t>& to_update, unsigned int level, bool force = false) const
      {
        // Update depth, if needed
        if (_needs_update)
          _update();

        // Initialize empty queue for elements that need to be updated (the queue uses depth as "priority" measure)
        std::priority_queue<std::pair<unsigned int, size_t>> queue;

        // Enqueue symbols that really need to be updated
        for (size_t i : to_update)
        {
          // cexps are pushed to the queue only if their value has changed
          if (force || !evaluated() || changed(i, level))
          {
            const std::shared_ptr<CExp<T>>& cur = (*this)[i];
            queue.push(std::make_pair(cur->depth, i));
          }
        }

        // Evaluate until queue is empty
        while (!queue.empty())
        {
          // Dequeue element
          size_t cur_i = queue.top().second;
          queue.pop();

          // Get corresponding compiled expression, evaluate it on level (no diff)
          const std::shared_ptr<CExp<T>>& cur = (*this)[cur_i];
          cur->compute(level);

          // If value changes, parents are enqueued
          if (force || !evaluated() || changed(cur_i, level))
            for (size_t i : cur->parents)
            {
              const std::shared_ptr<CExp<T>>& par = (*this)[i];
              queue.push(std::make_pair(par->depth, i));
            }
        }

        // Mark ValueStore as evaluated at least one
        _evaluated = true;
      }

      /** Recompute the (maximum) depth each expression.
          @param root index of the root element
          @param current_depth accumulator for the recursive call
       */
      inline void _update_depth(size_t root, unsigned int current_depth) const
      {
        auto& current_cexp = (*this)[root];
        current_cexp->depth = std::max(current_cexp->depth, current_depth);
        for (auto& c : current_cexp->children)
          _update_depth(c, current_depth + 1);
      }


      /** Map of all compiled expressions, for leaner compilation (expression reuse). */
      std::unordered_map<std::shared_ptr<const Exp<T>>, size_t, ExpHash<T>, ExpEquals<T>> _compiled;
      
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
      
      
      /** Whether the first full evaluation has been already run. */
      mutable bool _evaluated;
    
      /** Marks if ExpressionStore needs to recompute the depth of the compiled expressions. */
      mutable bool _needs_update;
      
    };
  }
}

