#ifndef _EXPRESSIONSTORE_HH
#define _EXPRESSIONSTORE_HH

#include "compiledexpression.hh"
#include <queue>
#include <unordered_map>

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

    /** Forward declaration */
    template <typename T>
    class ValueStore;

    class ResizeNotifier {
    public:
      virtual size_t size() const = 0;
    };

    /** Interface of a class who can be notified when a resize event happens. */
    class ResizeSubscriber
    {
    public:
      virtual void notify(const std::shared_ptr<ResizeNotifier>&) = 0;
    };

    /** ExpressionStore, a structure to handle bottom-up evaluation of compiled Exps. */
    template <typename T>
    class ExpressionStore : public std::vector<std::shared_ptr<CExp<T>>>, public Core::Printable, public ResizeNotifier, public std::enable_shared_from_this<ResizeNotifier>
    {

      /** FIXME: find out if this is needed */
      friend class ValueStore<T>;

    public:

      // using std::vector<std::shared_ptr<CExp<T>>>::size;
      inline size_t size() const
      {
        return std::vector<std::shared_ptr<CExp<T>>>::size();
      }

      /** Registers a subscriber for the resize event.
          @param n the ResizeNotify object to notify
       */
      inline void subscribe(const std::shared_ptr<ResizeSubscriber>& n) const
      {
        subscribers.push_back(n);
      }

      /** Compile an expression into a compiled expression.
          @param e the original expression
          @return a shared pointer to the root of the compiled expression
       */
      std::shared_ptr<CExp<T>> compile(std::shared_ptr<Exp<T>>& e)
      {
        auto it = this->compiled_exps.find(e);
        if (it != this->compiled_exps.end())
          return (*this)[it->second];

        // Ensure expression is normalized
        e->normalize(true);

        // Compile expression
        size_t previous_size = this->size();
        size_t root_index = e->compile(*this);

        // Check if a new expression has been inserted by compilation (or whether an old one was reused)
        if (this->size() != previous_size)
        {
          // Alert subscribers (a ValueStore) that the size has changed
          for (const auto& n : subscribers)
            n->notify(this->shared_from_this());
          _depth_needs_update = true;
        }
        return (*this)[root_index];
      }

      /** Evaluates all the registered expressions within a given ValueStore at a given level.
          @param vs the ValueStore to use as data source and storage
          @param level level on which to evaluate
       */
      void evaluate(ValueStore<T>& vs, unsigned int level = 0, bool force = false) const
      {
        // Update depth if needed
        if (_depth_needs_update)
          compute_depth();

        // Make list of terminal expressions
        const size_t n = this->size();
        std::unordered_set<size_t> to_update;
        for (size_t i = 0; i < n; i++)
        {
          if (std::static_pointer_cast<CTerm<T>>((*this)[i]) == nullptr)
            continue;
          to_update.insert(i);
        }

        // Call evaluate, passing list of all terminals as list of all terminals to update
        evaluate(vs, to_update, level, force);
      }

      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of variables that have been changed (delta).
          @param vs the ValueStore to use as data source and storage
          @param variables a set of variables that have been changed
          @param level level on which to evaluate
       */
      void evaluate_diff(ValueStore<T>& vs, const std::unordered_set<size_t>& to_update, unsigned int level) const
      {
        // Update depth if needed
        if (_depth_needs_update)
          compute_depth();

        // Initialize empty queue
        std::priority_queue<std::pair<int, size_t>> queue;

        // Set of already processed nodes
        std::unordered_set<size_t> processed;
        processed.reserve(this->size());

        // Go through all terminal modified symbols
        for (auto t : to_update)
        {
          // If terminal hasn't really changed, continue (a move might actually not change a variable, for some reason)
          if (!vs.changed(t, level))
            continue;

          // Get compiled expression
          const std::shared_ptr<CExp<T>>& cur = (*this)[t];
          for (size_t i : cur->parents)
          {
            const std::shared_ptr<CExp<T>>& par = (*this)[i];

            // If parent has not been enqueued already
            if (processed.find(i) == processed.end())
            {
              queue.push(std::make_pair(par->depth, i));
              processed.insert(i);
            }
            vs.changed_children(i, level).insert(t);
          }
        }

        // Processe elements until queue is empty
        while (!queue.empty())
        {
          // Dequeue expression
          size_t cur_i = queue.top().second;
          queue.pop();

          // Get compiled expression
          const std::shared_ptr<CExp<T>>& cur = (*this)[cur_i];

          // Compute new value (with diff)
          cur->compute_diff(vs, level);

          // If value has changed, and parent is not enqueued already, enqueue it
          if (vs.changed(cur_i, level))
            for (size_t i : cur->parents)
            {
              const std::shared_ptr<CExp<T>>& par = (*this)[i];
              if (processed.find(i) == processed.end())
              {
                queue.push(std::make_pair(par->depth, i));
                processed.insert(i);
              }

              // Update list of changed children for parent
              vs.changed_children(i, level).insert(cur_i);
            }
        }
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os) const
      {
        // Update depth if needed
        if (_depth_needs_update)
          compute_depth();

        // Print all compiled expressions in the expression store
        for (size_t i = 0; i < this->size(); i++)
        {
          auto cexp = (*this)[i];
          os << cexp << std::endl;
        }
      }

      /** Cached version of depth update. */
      inline void compute_depth() const
      {
        if (_depth_needs_update)
          for (size_t i = 0; i < this->size(); i++)
            _compute_depth(i, 0);
        _depth_needs_update = false;
      }

      /** Map of all compiled expressions, for leaner compilation (expression reuse). */
      inline const std::unordered_map<std::shared_ptr<const Exp<T>>, size_t, ExpHash<T>, ExpEquals<T>>& compiled_expressions() const
      {
        return this->compiled_exps;
      }

    protected:

      /** Evaluates all the registered expressions within a given ValueStore at a given level and a given set of expressions whose value has changed (delta).
          @param vs the ValueStore to use as data source and storage
          @param expressions a set of expressions that have been changed
          @param level level on which to evaluate
          @remarks Internally used by the public evaluate method
       */
      void evaluate(ValueStore<T>& vs, const std::unordered_set<size_t>& to_update, unsigned int level, bool force = false) const
      {
        // Update depth, if needed
        if (_depth_needs_update)
          compute_depth();

        // Initialize empty queue for elements that need to be updated (the queue uses depth as "priority" measure)
        std::priority_queue<std::pair<unsigned int, size_t>> queue;

        // Enqueue symbols that really need to be updated
        for (size_t i : to_update)
        {
          // cexps are pushed to the queue only if their value has changed
          if (force || !vs.evaluated || vs.changed(i, level))
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
          cur->compute(vs, level);

          // If value changes, parents are enqueued
          if (force || !vs.evaluated || vs.changed(cur_i, level))
            for (size_t i : cur->parents)
            {
              const std::shared_ptr<CExp<T>>& par = (*this)[i];
              queue.push(std::make_pair(par->depth, i));
            }
        }

        // Mark ValueStore as evaluated at least one
        vs.evaluated = true;
      }

      /** Recompute the (maximum) depth each expression.
          @param root index of the root element
          @param current_depth accumulator for the recursive call
       */
      inline void _compute_depth(size_t root, unsigned int current_depth) const
      {
        auto& current_cexp = (*this)[root];
        current_cexp->depth = std::max(current_cexp->depth, current_depth);
        for (auto& c : current_cexp->children)
          _compute_depth(c, current_depth + 1);
      }

      /** Resize events subscribers */
      mutable std::list<std::shared_ptr<ResizeSubscriber>> subscribers;

      /** Marks if ExpressionStore needs to recompute the depth of the compiled expressions. */
      mutable bool _depth_needs_update;

      /** Map of all compiled expressions, for leaner compilation (expression reuse). */
      std::unordered_map<std::shared_ptr<const Exp<T>>, size_t, ExpHash<T>, ExpEquals<T>> compiled_exps;

    };
  }
}


#endif // _EXPRESSIONSTORE_HH
