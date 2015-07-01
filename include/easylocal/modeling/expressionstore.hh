#ifndef EXPRESSIONSTORE_HH
#define EXPRESSIONSTORE_HH

#include "symbols.hh"
#include <queue>


namespace EasyLocal {
  
  namespace Modeling {
    
    /** Forward declaration */
    template <typename T>
    class ValueStore;
    
    /** Interface of a class who can be notified when a resize event happens. */
    class ResizeNotify
    {
    public:
      virtual void resized(size_t new_size) = 0;
    };
    
    /** ExpressionStore, a structure to handle bottom-up evaluation of compiled Exps. */
    template <typename T>
    class ExpressionStore : public std::vector<std::shared_ptr<CExp<T>>>, public Core::Printable
    {
      friend class ValueStore<T>;
    public:
      /** Registers a subscriber for the resize event.
       @param n the ResizeNotify object to notify
       */
      void subscribe(ResizeNotify* n) const
      {
        subscribers.push_back(n);
      }
      
      /** Compile an expression into a compiled expression.
       @param e the original expression
       @return a shared pointer to the root of the compiled expression
       */
      std::shared_ptr<CExp<T>> compile(Exp<T>& e)
      {
        e.normalize();
        size_t previous_size = this->size();
        size_t root_index = e.compile(*this);
        if (this->size() != previous_size)
        {
          for (ResizeNotify* n : subscribers)
            n->resized(this->size());
          depth_needs_update = true;
        }
        processed_symbols.resize(this->size());
        return (*this)[root_index];
      }
      
      /** Evaluates all the registered expressions within a given ValueStore at a given level.
       @param st the ValueStore to use as data source and storage
       @param level level on which to evaluate
       */
      void evaluate(ValueStore<T>& st, unsigned int level = 0, bool force = false) const
      {
        if (depth_needs_update)
          compute_depth();
        const size_t n = this->size();
        std::set<size_t> all_terminal_symbols;
        for (size_t i = 0; i < n; i++)
        {
          const std::shared_ptr<CExp<T>>& current_sym = (*this)[i];
          if (std::dynamic_pointer_cast<CTerm<T>>(current_sym) == nullptr)
            continue;
          all_terminal_symbols.insert(i);
        }
        evaluate(st, all_terminal_symbols, level, force);
      }
      
      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of variables that have been changed (delta).
       @param st the ValueStore to use as data source and storage
       @param variables a set of variables that have been changed
       @param level level on which to evaluate
       */
      void evaluate_diff(ValueStore<T>& st, const std::set<size_t>& variables, unsigned int level) const
      {
        if (depth_needs_update)
          compute_depth();
        std::priority_queue<std::pair<int, size_t>> queue;
        std::fill(processed_symbols.begin(), processed_symbols.end(), false);
        for (size_t var : variables)
        {
          if (!st.changed(var, level))
            continue;
          const std::shared_ptr<CExp<T>>& current_var = (*this)[var];
          for (size_t i : current_var->parents)
          {
            const std::shared_ptr<CExp<T>>& parent_sym = (*this)[i];
            if (!processed_symbols[i])
            {
              queue.push(std::make_pair(parent_sym->depth, i));
              processed_symbols[i] = true;
            }
            st.changed_children(i, level).insert(var);
          }
        }
        while (!queue.empty())
        {
          size_t current_index = queue.top().second;
          queue.pop();
          const std::shared_ptr<CExp<T>>& current_sym = (*this)[current_index];
          current_sym->compute_diff(st, level);
          // parents are pushed to the queue only if the current expression value has changed and they have not been already queued
          if (st.changed(current_index, level))
            for (size_t i : current_sym->parents)
            {
              const std::shared_ptr<CExp<T>>& parent_sym = (*this)[i];
              if (!processed_symbols[i])
              {
                queue.push(std::make_pair(parent_sym->depth, i));
                processed_symbols[i] = true;
              }
              st.changed_children(i, level).insert(current_index);
            }
        }
      }
      
      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        if (depth_needs_update)
          compute_depth();
        for (size_t i = 0; i < this->size(); i++)
        {
          const CExp<T>& sym = *this->operator[](i);
          os << sym << std::endl;
        }
      }
      
      void compute_depth() const
      {
        if (depth_needs_update)
          for (size_t i = 0; i < this->size(); i++)
            update_depth(i, 0);
        depth_needs_update = false;
      }
      
    protected:
      
      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of expressions whose value has changed (delta).
       @param st the ValueStore to use as data source and storage
       @param expressions a set of expressions that have been changed
       @param level level on which to evaluate
       @remarks Internally used by the public evaluate method
       */
      void evaluate(ValueStore<T>& st, const std::set<size_t>& expressions, unsigned int level, bool force = false) const
      {
        if (depth_needs_update)
          compute_depth();
        //if (!force || st.evaluated)
        //  return;
        std::priority_queue<std::pair<int, size_t>> queue;
        for (size_t i : expressions)
        {
          // symbols are pushed to the queue only if their value has changed
          if (force || !st.evaluated || st.changed(i, level))
          {
            const std::shared_ptr<CExp<T>>& current_sym = (*this)[i];
            queue.push(std::make_pair(current_sym->depth, i));
          }
        }
        while (!queue.empty())
        {
          size_t current_index = queue.top().second;
          queue.pop();
          const std::shared_ptr<CExp<T>>& current_sym = (*this)[current_index];
          current_sym->compute(st, level);
          // parents are pushed to the queue only if the current expression value has changed
          if (force || !st.evaluated || st.changed(current_index, level))
            for (size_t i : current_sym->parents)
            {
              const std::shared_ptr<CExp<T>>& parent_sym = (*this)[i];
              queue.push(std::make_pair(parent_sym->depth, i));
            }
        }
        st.evaluated = true;
      }
      
      /** Recompute the (maximum) depth of the different symbols rooted at root. **/
      void update_depth(size_t root, unsigned int current_depth) const
      {
        auto& current_sym = (*this)[root];
        current_sym->depth = std::max(current_sym->depth, current_depth);
        for (auto& c : current_sym->children)
          update_depth(c, current_depth + 1);
      }
      
      /** Resize events subscribers */
      mutable std::list<ResizeNotify*> subscribers;
      
    public:
      mutable std::vector<bool> processed_symbols;
      
      mutable bool depth_needs_update;

      std::map<size_t, size_t> compiled_symbols;
    };
    
  }
}


#endif
