#if !defined(_AUTOSTATE_HH_)
#define _AUTOSTATE_HH_

#include "easylocal/utils/printable.hh"
#include "easylocal/modeling/expression.hh"
#include "easylocal/modeling/valuestore.hh"
#include "easylocal/modeling/expressionstore.hh"
#include "easylocal/modeling/change.hh"

namespace EasyLocal {
    
  namespace Modeling {
    
    /** A class which takes care of handling the compilation of an Expression and its connection with an ExpressionStore.
    @remarks Largely a calling convenience.
    */
    template <typename T>
    class CompiledExpression
    {
    public:
      
      /** Default constructor. */
      CompiledExpression() : compiled_exp(nullptr), exp_store(nullptr)
        { }
      
      /** Copy constructor.
      @param ce the compiled expression to copy
      */
      CompiledExpression(const CompiledExpression& ce) : compiled_exp(ce.compiled_exp), exp_store(ce.exp_store)
        { }
      
      /** Constructor. Takes an Expression (not compiled) and compiles it, also adding it to an ExpressionStore.
      @param ex the expression to compile
      @param exp_store the expression store to add the compiled expression to
      */
      CompiledExpression(Exp<T>& ex, const std::shared_ptr<ExpressionStore<T>>& exp_store) : exp_store(exp_store)
      {
        compiled_exp = exp_store->compile(ex);
      }
      
      /** Const cast operator to the actual expression.
      */
      operator const Sym<T>&() const
      {
        return *compiled_exp;
      }
      
      bool is_valid() const
      {
        return compiled_exp != nullptr;
      }
      
    protected:
      
      /** Pointer to the compiled expression. */
      std::shared_ptr<Sym<T>> compiled_exp;
      
      /** Pointer to the expression store where the expression is stored. */
      std::shared_ptr<ExpressionStore<T>> exp_store;
    };
    
    
    /** An "abstract" state whose deltas are computed based on CompiledExpressions.
    Provides methods to create "managed" decision variables and arbitrarily complex
    expressions which can be used as cost components or cost functions.
    */
    template <typename T>
    class AutoState : public virtual Printable
    {
    public:
      
      /** Constructor.
      @remarks Initializes an ExpressionStore, and a ValueStore supporting any number
      of evaluation scenarios (e.g., for simultaneous evaluation of multiple Changes
      on multiple threads).
      @param levels how many scenarios are supported by the ValueStore
      */
      AutoState(size_t levels = 1) : es(std::make_shared<ExpressionStore<T>>()), st(es, levels)
        { }
      
      /** Sets (in a definitive way) the value of one of the registered decision variables. */
      void set(const Var<T>& var, const T& val)
      {
        st.assign(var, 0, val);
      }
      
      /** Evaluates (completely) the registered CompiledExpressions. */
      void evaluate()
      {
        es->evaluate(st);
      }
      
      /** Gets the value of a CompiledExpression (possibly at a specific level).
      @param ce compiled expression to get the value for
      @param level level to get the values from
      */
      T value_of(const CompiledExpression<T>& ce, size_t level = 0) const
      {
        if (!ce.is_valid())
          throw std::logic_error("Trying to access an unassigned compiled expression");
        return st(ce, level);
      }
      
      /** Gets the value of a Variable (possibly at a specific level).
      @param ce compiled expression to get the value for
      @param level level to get the values from
      */
      T value_of(const Var<T>& v, size_t level = 0) const
      {
        return st(v, level);
      }
      
      /** Simulates the execution of a simple Change on a specific simulation level.
      @param m the Change to simulate
      @param level level onto which the Change must be simulated
      */
      void simulate(const BasicChange<T>& m, size_t level = 1) const
      {
        if (level == 0)
          throw std::logic_error("Cannot simulate at level 0");
        const_cast<ValueStore<T>&>(st).simulate(m, level);
      }
      
      /** Simulates the execution of a composite Change on a specific simulation level.
      @param m the Change to simulate
      @param level level onto which the Change must be simulated
      */
      void simulate(const CompositeChange<T>& m, size_t level = 1) const
      {
        if (level == 0)
          throw std::logic_error("Cannot simulate at level 0");
        const_cast<ValueStore<T>&>(st).simulate(m, level);
      }
      
      /** Executes a simple Change.
      @param m the Change to execute
      */
      void execute(const BasicChange<T>& m)
      {
        st.execute(m);
      }
      
      /** Executes a composite Change.
      @param m the Change to execute
      */
      void execute(const CompositeChange<T>& m)
      {
        st.execute(m);
      }
      
      /** @copydoc Printable::print(std::ostream&) */
      virtual void print(std::ostream& os = std::cout) const
      {
        os << st;
      }
      
    protected:
      
      /** Takes an expression and compiles it on the AutoState's ExpressionStore.
      @param e expression to compile
      */
      CompiledExpression<T> compile(Exp<T>& e)
      {
        return CompiledExpression<T>(e, es);
      }
      
      /** Generates a scalar value, and registers it in the ExpressionStore.
      @param name name of the variable to generate
      */
      Var<T> make_scalar(const std::string& name)
      {
        return Var<T>(es, name);
      }
      
      /** Generates a vector value, and registers it in the ExpressionStore.
      @param name name of the variable to generate
      @param size size of the vector of variables
      */
      VarArray<T> make_array(const std::string& name, size_t size)
      {
        VarArray<T> v(*es, name, size);
        return v;
      }
      
      /** The AutoState's ExpressionStore */
      std::shared_ptr<ExpressionStore<T>> es;
      
      /** The AutoState's ValueStore (inner state) */
      ValueStore<T> st;
    };
  }
}

#endif // _AUTOSTATE_HH_