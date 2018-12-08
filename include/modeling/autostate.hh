#pragma once

#include "easylocal/utils/printable.hh"
#include "expression.hh"
#include "expressionstore.hh"
#include "change.hh"

namespace EasyLocal {

  namespace Modeling {

    /** An "abstract" state whose deltas are computed based on CompiledExpressions.
        Provides methods to create "managed" decision variables and arbitrarily complex
        expressions which can be used as cost components or cost functions.
     */
    template <typename T>
    class AutoState : public virtual Core::Printable
    {
    public:
      AutoState(size_t levels = 1) : es(std::make_shared<ExpressionStore<T>>()), vs(*es, levels)
      {}

      /** Constructor.
       @remarks Initializes an ExpressionStore, and a ValueStore supporting any number
       of evaluation scenarios (e.g., for simultaneous evaluation of multiple Changes
       on multiple threads).
       @param levels how many scenarios will be supported by the ValueStore
       */
      AutoState(std::shared_ptr<ExpressionStore<T>> es, size_t levels = 1) : es(es), vs(*es, levels)
      {}

      AutoState(std::shared_ptr<ExpressionStore<T>> es, const ValueStore<T>& vs) : es(es), vs(vs)
      {}

      /** Sets (in a definitive way) the value of one of the registered decision variables. */
      void set(const Var<T>& var, const T& val)
      {
        vs.assign(var, 0, val);
      }

      /** Evaluates (completely) the registered CompiledExpressions. */
      void evaluate(bool force = false)
      {
        es->evaluate(vs, 0, force);
      }

      /** Gets the value of a CompiledExpression (possibly at a specific level).
       @param ce compiled expression to get the value for
       @param level level to get the values from
       */
      T value_of(const CompiledExpression<T>& ce, size_t level = 0) const
      {
        if (!ce.is_valid())
          throw std::logic_error("Trying to access an unassigned compiled expression");
        return vs(ce, level);
      }

      /** Gets the value of a Symbol (possibly at a specific level).
       @param s the symbol to get the value for
       @param level level to get the values from
       */
      T value_of(const CExp<T>& s, size_t level = 0) const
      {
        return vs(s, level);
      }

      /** Gets the value of a compiled location (possibly at a specific level).
       @param i the location to get the value for
       @param level level to get the values from
       */
      T value_of(size_t i, size_t level = 0) const
      {
        return vs(i, level);
      }

      /** Gets the value of a Variable (possibly at a specific level).
       @param v the variable to get the value for
       @param level level to get the values from
       */
      T value_of(const Var<T>& v, size_t level = 0) const
      {
        return vs(v, level);
      }

      /** Simulates the execution of a simple Change on a specific simulation level.
       @param m the Change to simulate
       @param level level onto which the Change must be simulated
       */
      void simulate(const BasicChange<T>& m, size_t level = 1) const
      {
        if (level == 0)
          throw std::logic_error("Cannot simulate at level 0");
        const_cast<ValueStore<T>&>(vs).simulate(m, level);
      }

      /** Simulates the execution of a composite Change on a specific simulation level.
       @param m the Change to simulate
       @param level level onto which the Change must be simulated
       */
      void simulate(const CompositeChange<T>& m, size_t level = 1) const
      {
        if (level == 0)
          throw std::logic_error("Cannot simulate at level 0");
        const_cast<ValueStore<T>&>(vs).simulate(m, level);
      }

      /** Executes a simple Change.
       @param m the Change to execute
       */
      void execute(const BasicChange<T>& m)
      {
        vs.execute(m);
      }

      /** Executes a composite Change.
       @param m the Change to execute
       */
      void execute(const CompositeChange<T>& m)
      {
        vs.execute(m);
      }

      /** @copydoc Printable::Print(std::ostream&) */
      virtual void Print(std::ostream& os = std::cout) const
      {
        os << vs;
      }

    protected:

      /** Takes an expression and compiles it on the AutoState's ExpressionStore.
       @param e expression to compile
       */
      CompiledExpression<T> compile(Exp<T>& e)
      {
        return CompiledExpression<T>(e, *es);
      }

      /** Generates a scalar value, and registers it in the ExpressionStore.
       @param name name of the variable to generate
       */
      Var<T> make_scalar(const std::string& name, T lb, T ub)
      {
        return Var<T>(es, name, lb, ub);
      }

      /** Generates a vector value, and registers it in the ExpressionStore.
       @param name name of the variable to generate
       @param size size of the vector of variables
       */
      VarArray<T> make_array(const std::string& name, size_t size, T lb, T ub)
      {
        VarArray<T> v(*es, name, size);
        return v;
      }

      /** The AutoState's ExpressionStore */
      std::shared_ptr<ExpressionStore<T>> es;

    public:
      /** The AutoState's ValueStore (inner state) */
      ValueStore<T> vs;
    };
  }
}
