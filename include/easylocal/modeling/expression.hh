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
#include "easylocal/utils/random.hh"

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
      
      Exp(const std::shared_ptr<ASTItem<T>>& p_ai) : p_ai(p_ai) {}
      
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
      {}
      
      size_t hash() const
      {
        return p_ai->hash();
      }
      
      size_t compile(ExpressionStore<T>& exp_store) const
      {
        return p_ai->compile(exp_store);
      }
      
      bool is_empty() const
      {
        return p_ai == nullptr;
      }
    };
    
    template <typename T>
    class VarArray;
    
    /**
     A modeling variable to be used inside expressions.     
     */
    template <typename T>
    class Var : public Exp<T>
    {
      friend class VarArray<T>;
      
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
      
      virtual void Print(std::ostream& os) const
      {
        Exp<T>::Print(os);
        os << " " << domain;
      }
      
      void clearDomain()
      {
        domain = Domain();
      }
      
      void setDomain(T lb, T ub) throw (EmptyDomain)
      {
        domain.clear();
        domain.insert(boost::icl::interval<T>::closed(lb, ub));
        if (domain.empty())
        {
          throw EmptyDomain(this->name());
        }
      }
      
      void addToDomain(T val)
      {
        domain.insert(boost::icl::interval<T>::closed(val, val));
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
      
      T min() const
      {
        return *boost::icl::elements_begin(domain);
      }
      
      T max() const
      {
        return *boost::icl::elements_rbegin(domain);
      }
      
      T med() const
      {
        // TODO: find a more efficient way to generate the median value in the domain
        size_t r = domain.size() / 2;
        auto it = boost::icl::elements_begin(domain);
        for (size_t i = 0; i < r; ++i, ++it)
          ;
        return *it;
      }
      
      T rand() const
      {
        // TODO: find a more efficient way to generate a random value in the domain
        size_t r = Core::Random::Int(0, domain.size() - 1);
        auto it = boost::icl::elements_begin(domain);
        for (size_t i = 0; i < r; ++i, ++it)
          ;
        return *it;
      }
      
      DomainIterator begin() const
      {
        return boost::icl::elements_begin(domain);
      }
      
      DomainIterator end() const
      {
        return boost::icl::elements_end(domain);
      }
      
      bool assigned() const
      {
        return domain.size() == 1;
      }      
      
    protected:
      Domain domain;
    };

    template <typename T>
    bool operator==(const Var<T>& v1, const Var<T>& v2)
    {
      return v1.p_ai == v2.p_ai;
    }

    template <typename T>
    bool operator<(const Var<T>& v1, const Var<T>& v2)
    {
      return v1.p_ai < v2.p_ai;
    }
    
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
      
      Exp<T> operator[](const Exp<T>& index)
      {
        Exp<T> t = Exp<T>(std::make_shared<Element<T>>(index, *this));
        t.simplify();
        return t;
      }
      
      virtual void Print(std::ostream& os) const
      {
        Exp<T>::Print(os);
        for (size_t i = 0; i < this->size(); i++)
          os << " " << (*this)[i].domain;
      }
    };
    
    /** The function for setting a variable domain */
    template <typename T>
    void dom(Var<T>& v, T lb, T ub)
    {
      v.setDomain(lb, ub);
    }
  }
}


#endif
