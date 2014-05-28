#if !defined(_EXPRESSION_HH_)
#define _EXPRESSION_HH_

#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

#include "modeling/Printable.hh"
#include "modeling/AST.hh"
#include "modeling/Operators.hh"

namespace easylocal
{
  namespace modeling
  {    
    template <typename T>
    class Exp : public virtual Printable
    {
    public:
      Exp() : p_ai(nullptr)
      {
        //std::cout << "Created Exp from scratch." << std::endl;
      }
      
      Exp(const T& value) : p_ai(std::make_shared<ASTConst<T>>(value))
      {
      
      }
        
      Exp(const Exp<T>& other)
      {
        p_ai = other.p_ai;
      }

      Exp(Exp<T>&& other) : Exp<T>()
      {
        swap(*this, other);
      }

      
      Exp<T>& operator=(Exp<T> other) // (1)
      {
        swap(*this, other); // (2)
        return *this;
      }
      
      friend void swap(Exp<T>& first, Exp<T>& second) // nothrow
      {
        using std::swap;
        swap(first.p_ai, second.p_ai);
      }
      
      Exp(const std::shared_ptr<ASTItem<T>>& p_ai) : p_ai(p_ai) { }
      
      virtual void print(std::ostream& os) const
      {
        p_ai->print(os);
      }
      
      void simplify()
      {
        p_ai = p_ai->simplify();
      }
      
      void normalize()
      {
        p_ai->normalize(true);
      }

      std::shared_ptr<ASTItem<T>> p_ai;

      virtual ~Exp<T>()
      {
        //if (p_ai != nullptr)
        //  ASTItem<T>::erase(p_ai);
        //std::cout << "Destructing (Exp<T>) " << this << std::endl;
      }
      
      size_t hash() const
      {
        return p_ai->hash();
      }
      
      size_t compile(ExpressionStore<T>& exp_store) const
      {
        return p_ai->compile(exp_store);
      }
      
    };

    
    /**
     A modeling variable to be used inside expressions.
     @remarks extends Symbol
     */
    template <typename T>
    class Var : public Exp<T>
    {
    public:
      /**
       Constructor.
       @param exp_store a reference to the ExpressionStore (compiled expression) where the variable will be registered
       @param name name of the variable (for printing purposes)
       */
      explicit Var(ExpressionStore<T>& exp_store, const std::string& name)
      {
        std::shared_ptr<ASTVar<T>> var = std::make_shared<ASTVar<T>>(name);
        this->p_ai = var;
        var->compile(exp_store);
      }
      
      Var() = default;
      
      /**
       Copy constructor.
       @param v the variable to copy
       */
      Var(const Var& v) = default;

      virtual ~Var() = default;
    };
    
    /**
     A variable array to facilitate the initialization of sequences of variables.
     @remarks extends ginac::symbol
     */
    template <typename T>
    class VarArray : public Exp<T>, public std::vector<Var<T>>
    {
    public:
      /**
       Constructor.
       @param exp_store a reference to the ExpressionStore (compiled expression) where the variable will be registered
       @param name name of the variable array
       @param size size of the variable array to declare
       */
      explicit VarArray(ExpressionStore<T>& exp_store, const std::string& name, size_t size)
      {
        std::shared_ptr<ASTVarArray<T>> var_array = std::make_shared<ASTVarArray<T>>(name, size);
        this->p_ai = var_array;
        var_array->compile(exp_store);
        this->reserve(size);
        for (size_t i = 0; i < size; i++)
        {
          std::ostringstream os;
          os << name << "[" << i << "]";
          this->push_back(Var<T>(exp_store, os.str()));
        }
      }
      
      virtual ~VarArray() = default;
      
      /**
       Copy Constructor.
       @param the passed variable
       */
      VarArray(const VarArray& v) = default;
      
      /** Forwards access to the vector */
      using std::vector<Var<T>>::operator[];
      
      VarArray() = default;
      
    };
  }
}


#endif // _EXPRESSION_HH_