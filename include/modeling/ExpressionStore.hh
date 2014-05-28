#if !defined(_EXPRESSIONSTORE_HH_)
#define _EXPRESSIONSTORE_HH_

#include <queue>

#include "modeling/Symbols.hh"

namespace easylocal {
  namespace modeling {
    
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
    class ExpressionStore : public std::vector<std::shared_ptr<Sym<T>>>, public Printable
    {
      friend class ValueStore<T>;
    public:
      
      /** Registers a subscriber for the resize event.
       @param n the ResizeNotify object to notify
       */
      void subscribe(ResizeNotify* n)
      {
        subscribers.push_back(n);
      }
      
      /** Compile an expression into a compiled expression.
       @param e the original expression
       @return a pointer to the root of the compiled expression
       */
      std::shared_ptr<Sym<T>> compile(Exp<T>& e)
      {
        e.normalize();
        tree_depth = 0;
        size_t previous_size = this->size();
        size_t root_index = e.compile(*this);
        if (this->size() != previous_size)
          for (ResizeNotify* n : subscribers)
            n->resized(this->size());
        processed_symbols.resize(this->size());
        return (*this)[root_index];
      }
      
      /** Evaluates all the registered expressions within a given ValueStore at a given level.
       @param st the ValueStore to use as data source and storage
       @param level level on which to evaluate
       */
      void evaluate(ValueStore<T>& st, unsigned int level = 0) const
      {
        const size_t n = this->size();
        std::set<size_t> all_terminal_symbols;
        for (size_t i = 0; i < n; i++)
        {
          const std::shared_ptr<Sym<T>>& current_sym = (*this)[i];
          if (std::dynamic_pointer_cast<TermSym<T>>(current_sym) == nullptr)
            continue;
          all_terminal_symbols.insert(i);
        }
        evaluate(st, all_terminal_symbols, level);
      }
      
      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of variables that have been changed (delta).
       @param st the ValueStore to use as data source and storage
       @param variables a set of variables that have been changed
       @param level level on which to evaluate
       */
      void evaluate_diff(ValueStore<T>& st, const std::set<size_t>& variables, unsigned int level) const
      {
        std::priority_queue<std::pair<int, size_t>> queue;
        std::fill(processed_symbols.begin(), processed_symbols.end(), false);
        for (const size_t& var : variables)
        {
          if (!st.changed(var, level))
            continue;
          const std::shared_ptr<Sym<T>>& current_var = (*this)[var];
          for (const size_t& i : current_var->parents)
          {
            const std::shared_ptr<Sym<T>>& parent_sym = (*this)[i];
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
          const std::shared_ptr<Sym<T>>& current_sym = (*this)[current_index];
          current_sym->compute_diff(st, level);
          // parents are pushed to the queue only if the current expression value has changed and they have not been already queued
          if (st.changed(current_index, level))
            for (const size_t& i : current_sym->parents)
            {
              const std::shared_ptr<Sym<T>>& parent_sym = (*this)[i];
              if (!processed_symbols[i])
              {
                queue.push(std::make_pair(parent_sym->depth, i));
                processed_symbols[i] = true;
              }
              st.changed_children(i, level).insert(current_index);
            }
        }
      }
      
      /** @copydoc Printable::print(std::ostream&) */
      virtual void print(std::ostream& os) const
      {
        for (size_t i = 0; i < this->size(); i++)
        {
          const Sym<T>& sym = *this->operator[](i);
          os << sym << std::endl;
        }
      }
      
    protected:
      
      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of expressions whose value has changed (delta).
       @param st the ValueStore to use as data source and storage
       @param expressions a set of expressions that have been changed
       @param level level on which to evaluate
       @remarks Internally used by the public evaluate method
       */
      void evaluate(ValueStore<T>& st, const std::set<size_t>& expressions, unsigned int level) const
      {
        std::queue<size_t> queue;
        for (const size_t& i : expressions)
        {
          // variables are pushed to the queue only if their value has changed
          if (!st.evaluated || st.changed(i, level))
            queue.push(i);
        }
        while (!queue.empty())
        {
          size_t current_index = queue.front();
          queue.pop();
          const std::shared_ptr<Sym<T>>& current_sym = (*this)[current_index];
          current_sym->compute(st, level);
          // parents are pushed to the queue only if the current expression value has changed
          if (!st.evaluated || st.changed(current_index, level))
            for (const size_t& i : current_sym->parents)
              queue.push(i);
        }
        st.evaluated = true;
      }
      
      /** Resize events subscribers */
      std::list<ResizeNotify*> subscribers;
      
    public:
      
      /** Maximum depth of the tree */
      mutable unsigned int tree_depth;

      mutable std::vector<bool> processed_symbols;

      std::map<size_t, size_t> compiled_symbols;
    };
    
  }
}


#endif // _EXPRESSIONSTORE_HH_