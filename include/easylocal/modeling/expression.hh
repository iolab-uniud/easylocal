#ifndef _EXPRESSION_HH
#define _EXPRESSION_HH

#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

#include "easylocal/utils/printable.hh"
#include "ast.hh"
#include "operators.hh"
#include <boost/icl/interval_set.hpp>
#include <iterator>
#include "easylocal/utils/types.hh"

namespace EasyLocal
{
  namespace Modeling
  {
    
    
    /** An exception in case of incorrect variable domain */
    class EmptyDomain : public std::logic_error
    {
    public:
      EmptyDomain(const std::string& what) : std::logic_error(what.c_str()) {}
    };
    
    template <typename T>
    class Exp : public virtual Core::Printable
    {
    public:
      Exp() : p_ai(nullptr)
      {}
      
      Exp(T value) : p_ai(std::make_shared<ASTConst<T>>(value))
      {}
        
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
      
      virtual void Print(std::ostream& os) const
      {
        p_ai->Print(os);
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
    
    template <typename T>
    class DomainIterator;
    
    /**
     A modeling variable to be used inside expressions.
     @remarks extends Symbol
     */
    template <typename T>
    class Var : public Exp<T>
    {
      friend class DomainIterator<T>;
      
      typedef boost::icl::interval_set<T> Domain;
      typedef typename boost::icl::interval_set<T>::element_const_iterator DomainIterator;
    public:
      /**
       Constructor.
       @param exp_store a reference to the ExpressionStore (compiled expression) where the variable will be registered
       @param name name of the variable (for printing purposes)
       */
      explicit Var(ExpressionStore<T>& exp_store, const std::string& name, T lb, T ub) throw (EmptyDomain)
      {
        std::shared_ptr<ASTVar<T>> var = std::make_shared<ASTVar<T>>(name);
        this->p_ai = var;
        setDomain(lb, ub);
        if (domain.empty())
        {
          throw EmptyDomain(this->name());
        }
        var->compile(exp_store);
      }
      
      Var() = default;
      
      /**
       Copy constructor.
       @param v the variable to copy
       */
      Var(const Var& v) = default;

      virtual ~Var() = default;
      
      const std::string& name() const
      {
        ASTVar<T>* p_av = dynamic_cast<ASTVar<T>*>(this->p_ai.get());
        return p_av->name;
      }
      
      void setStep(T step)
      {
        this->step = step;
      }
      
      void setDomain(T lb, T ub) throw (EmptyDomain)
      {
        domain.insert(boost::icl::interval<T>::closed(lb, ub));
        if (domain.empty())
        {
          throw EmptyDomain(this->name());
        }
      }
      
      void removeFromDomain(T val) throw (EmptyDomain)
      {
        domain.erase(val);
        if (domain.empty())
        {
          throw EmptyDomain(this->name());
        }
      }
      
      void removeFromDomain(T lb, T ub) throw (EmptyDomain)
      {
        domain.erase(boost::icl::interval<T>::closed(lb, ub));
        if (domain.empty())
        {
          throw EmptyDomain(this->name());
        }
      }
      
      bool inDomain(T val) const
      {
        return boost::icl::contains(domain, val);
      }
      
      DomainIterator begin() const
      {
        return boost::icl::elements_begin(domain);
      }
      
      DomainIterator end() const
      {
        return boost::icl::elements_end(domain);
      }
    protected:
      Domain domain;
    };
    
    /**
     Iterator for the variable domain.
     */
    /* template <typename T>
    class DomainIterator : public std::iterator<std::input_iterator_tag, T>
    {
      friend class Var<T>;
    public:
      DomainIterator operator++(int) // postfix
      {
        DomainIterator pi = *this;
        if (current_interval_it == var.domain.end())
          throw std::logic_error("Attempting to go after last move");
        if (value + step > current_interval_it->upper())
        {
          ++current_interval_it;
          if (current_interval_it != var.domain.end())
            value = current_interval_it->lower();
        }
        else
          value += step;
        return pi;
      }
      DomainIterator& operator++() // prefix
      {
        if (current_interval_it == var.domain.end())
          throw std::logic_error("Attempting to go after last move");
        if (value + step > current_interval_it->upper())
        {
          ++current_interval_it;
          if (current_interval_it != var.domain.end())
            value = current_interval_it->lower();
        }
        else
          value += step;
        return *this;
      }
      T operator*() const
      {
        return value;
      }
      T* operator->() const
      {
        return &value;
      }
      bool operator==(const DomainIterator& it2) const
      {
        return current_interval_it == it2.current_interval_it && &this->var == &it2.var && value == it2.value;
      }
      bool operator!=(const DomainIterator& it2)
      {
        return current_interval_it != it2.current_interval_it || &this->var != &it2.var || (current_interval_it != var.domain.end() && value != it2.value);
      }
    protected:
      DomainIterator(const Var<T>& var, T step)
      : var(var), step(step), value(var.domain.front().lower()), current_interval_it(var.domain.begin())
      {}
      const Var<T>& var;
      T step, value;
      typename std::list<typename Var<T>::Interval>::const_iterator current_interval_it;
    };
     
     */

    
    /**
     A variable array to facilitate the initialization of sequences of variables.
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
      explicit VarArray(ExpressionStore<T>& exp_store, const std::string& name, size_t size, T lb, T ub)
      {
        std::shared_ptr<ASTVarArray<T>> var_array = std::make_shared<ASTVarArray<T>>(name, size);
        this->p_ai = var_array;
        var_array->compile(exp_store);
        this->reserve(size);
        for (size_t i = 0; i < size; i++)
        {
          std::ostringstream os;
          os << name << "[" << i << "]";
          this->emplace_back(exp_store, os.str(), lb, ub);
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
    
    /** The function for setting a variable domain */
    template <typename T>
    void dom(Var<T>& v, T lb, T ub)
    {
      v.dom(lb, ub);
    }
  }
}


#endif
