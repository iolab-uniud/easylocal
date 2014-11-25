#ifndef _AST_HH
#define _AST_HH

#include "easylocal/utils/printable.hh"
#include <memory>
#include <vector>
#include <list>
#include <map>
#include "symbols.hh"

// TODO: perform syntactic check for subexpressions (i.e., filter out incompatible subexpressions)

namespace EasyLocal {
  
  namespace Modeling {
    
    template <typename T>
    class ASTVarArray;
    
    template <typename T>
    class ASTItem : public virtual Core::Printable, public std::enable_shared_from_this<ASTItem<T>>
    {
    public:
      
      ASTItem() : _hash_set(false), _simplified(false), _normalized(false) { }
            
      virtual ~ASTItem() = default;
      
      size_t hash() const
      {
        if (!_hash_set)
        {
          this->_hash = compute_hash();
          this->_hash_set = true;
        }
        return _hash;
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        this->_simplified = true;
        return this->shared_from_this();
      }
    
      const std::type_info& type() const
      {
        return typeid(*this);
      }
      
      bool simplified() const
      {
        return _simplified;
      }
      
      bool normalized() const
      {
        return _normalized;
      }
      
      virtual void normalize(bool recursive)
      {
        this->_normalized = true;
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const = 0;
      
      
    protected:
      virtual void check_compatibility(const std::shared_ptr<ASTItem<T>> sub_ex, size_t pos = 0) const
      {
        // In general expressions are not compatible with var array operands unless specified
        if (sub_ex->type() == typeid(ASTVarArray<T>))
        {
          std::ostringstream os;
          os << sub_ex << " type is incompatible with " << *this;
          throw std::logic_error(os.str());
        }
      }
      
      template <template <typename> class CType>
      std::pair<size_t, std::shared_ptr<CType<T>>> get_or_create(ExpressionStore<T>& exp_store) const
      {
        auto it = exp_store.compiled_symbols.find(this->hash());
        if (it != exp_store.compiled_symbols.end())
          return std::make_pair(it->second, nullptr);
        size_t this_index = exp_store.size();
        exp_store.compiled_symbols[this->hash()] = this_index;
        std::shared_ptr<CType<T>> compiled = std::make_shared<CType<T>>(exp_store);
        exp_store.push_back(compiled);
        exp_store[this_index]->index = this_index;
        return std::make_pair(this_index, compiled);
      }
      
      mutable bool _hash_set;
      mutable size_t _hash;
      bool _simplified, _normalized;
      virtual size_t compute_hash() const = 0;
    };
    
    template <typename T>
    class ASTStable : public ASTItem<T>
    {
    public:
      virtual ~ASTStable() = default;
      
    protected:
      ASTStable()
      {
        this->_simplified = true;
        this->_normalized = true;
      }
    };
    
    template <typename T>
    class ASTVar : public ASTStable<T>
    {
    public:
      
      ASTVar(const std::string& name) : name(name)
      {}
      
      virtual void Print(std::ostream& os) const
      {
        os << name;
      }
      
      virtual ~ASTVar() = default;
      
      const std::string name;
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        return this->template get_or_create<VarSym>(exp_store).first;
      }
      
    protected:
      
      virtual size_t compute_hash() const
      {
        return std::hash<std::string>()(this->name);
      }
    };
    
    template <typename T>
    class ASTVarArray : public ASTStable<T>
    {
    public:
      
      ASTVarArray(const std::string& name, size_t size) : name(name), size(size)
      { }
      
      virtual void Print(std::ostream& os) const
      {
        os << this->name << "{size="<< this->size << "}";
      }
      
      virtual ~ASTVarArray() = default;
      
      const std::string name;
      
      const size_t size;      
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<VarArraySym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<VarArraySym<T>>(compiled_pair.second);
          compiled->start = 0;
          compiled->size = size;
        }
        return compiled_pair.first;
      }

    protected:
      virtual size_t compute_hash() const
      {
        return std::hash<std::string>()(this->name);
      }
    };
    
    template <typename T>
    class ASTConst : public ASTStable<T>
    {
    public:
      
      ASTConst(const T& value) : value(value)
      {}
      
      virtual void Print(std::ostream& os) const
      {
        os << "C(" << (T)(this->value) << ")";
      }
      
      virtual ~ASTConst() = default;
      
      const T value;
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<ConstSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<ConstSym<T>>(compiled_pair.second);
          compiled->value = this->value;
        }
        return compiled_pair.first;
      }
      
    protected:
      
      virtual size_t compute_hash() const
      {
        return std::hash<T>()(this->value);
      }
    };
    
    /** Compare function for normalization. */
    template <typename T>
    bool _normalize(const std::shared_ptr<ASTItem<T>>& o1, const std::shared_ptr<ASTItem<T>>& o2)
    {
      return (o1->type().hash_code() < o2->type().hash_code()) || (o1->type().hash_code() == o2->type().hash_code() && o1->hash() < o2->hash());
    }

    
    template <typename T>
    class ASTOp : public ASTItem<T>
    {
    public:
      virtual void Print(std::ostream& os) const
      {
        os << "(";
        int count = 0;
        for (auto& op : this->operands)
        {
          count++;
          op->Print(os);
          if (count < operands.size())
            os << " " << this->sym << " ";
        }
        os << ")";
      }
      
      virtual void append_operand(const Exp<T>& operand)
      {
        this->check_compatibility(operand.p_ai);
        this->operands.push_back(operand.p_ai);
      }
      
      virtual ~ASTOp() = default;
      
      virtual void normalize(bool recursive)
      {
        if (this->normalized())
          return;
        if (recursive)
          for (auto& op : this->operands)
            op->normalize(recursive);
        this->operands.sort(&_normalize<T>);
      }
      

      const std::list<std::shared_ptr<ASTItem<T>>>& ops() const
      {
        return operands;
      }
      
      virtual T steal_const(T def)
      {
        std::shared_ptr<ASTItem<T>> front = this->operands.front();
        
        if (front->type() == typeid(ASTConst<T>))
        {
          this->operands.pop_front();
          return std::dynamic_pointer_cast<ASTConst<T>>(front)->value;
        }
        
        return def;
      }
      
    protected:
      
      virtual void add_operand(const std::shared_ptr<ASTItem<T>>& o)
      {
        this->check_compatibility(o);
        this->operands.push_back(o);
      }
      
      virtual void add_constant(const std::shared_ptr<ASTConst<T>>& o)
      {
        this->operands.push_front(o);
      }
      
      virtual void merge_operands(const std::shared_ptr<ASTItem<T>>& other)
      {
        // tentative reverse implementation
        ASTOp<T>* p_other = dynamic_cast<ASTOp<T>*>(other.get());
        if (p_other == nullptr)
          return;
        
        // reverse with swap is generally faster
        p_other->operands.splice(p_other->operands.end(), this->operands);
        std::swap(this->operands, p_other->operands);
      }
      
      ASTOp(const std::string& sym) : sym(sym) { }
      
      static inline size_t rol(size_t x, size_t places = 1)
      {
        return (x << places) | (x >> (sizeof(size_t) * 8 - places));
      }
      
      virtual size_t compute_hash() const
      {
        size_t h = std::hash<std::string>()(this->sym);
        
        for(const std::shared_ptr<ASTItem<T>>& op : operands)
        {
          h = rol(h);
          h ^= op->hash();
        }
        
        return h;
      }

      void compile_operands(size_t this_index, ExpressionStore<T>& exp_store) const
      {
        for (auto& op : this->operands)
        {
          size_t child_index = op->compile(exp_store);
          exp_store[this_index]->children.push_back(child_index);
          exp_store[child_index]->parents.insert(this_index);
        }
      }
      
      std::string sym;
      
    public:
      std::list<std::shared_ptr<ASTItem<T>>> operands;
    };
    
    template <typename T>
    class Sum : public ASTOp<T>
    {
    public:

      Sum(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("+")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        // lift sum of sums
        T sum_of_const = (T)0;
        
        // ALL AT ONCE (simplify, steal and aggregate constants, inherit operands)
        for (auto it = this->operands.begin(); it != this->operands.end();)
        {
          // correct way to replace element in list
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // if a sum is detected, add its operands to the end, then erase it
          if ((*it)->type() == typeid(Sum<T>))
          {
            sum_of_const += std::dynamic_pointer_cast<ASTOp<T>>(*it)->steal_const(0);
            this->merge_operands(*it);
            it = this->operands.erase(it);
          }
          // if a constant is detected, add its value to the sum, then erase it
          else if ((*it)->type() == typeid(ASTConst<T>))
          {
            sum_of_const += std::dynamic_pointer_cast<ASTConst<T>>(*it)->value;
            it = this->operands.erase(it);
          }
          else
          {
            ++it;
          }
        }
        
        // add
        if (sum_of_const != 0)
          this->add_constant(std::make_shared<ASTConst<T>>(sum_of_const));
        
        // zero elements sum
        if (this->operands.size() == 0)
          return std::make_shared<ASTConst<T>>(0);
        
        // one-element sum
        if (this->operands.size() == 1)
          return this->operands.front();
        
        this->_simplified = true;
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<SumSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<SumSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~Sum() = default;
    };
    
    template <typename T>
    class Mul : public ASTOp<T>
    {
    public:
      
      Mul(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("*")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
  
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        // lift product of products
        T prod_of_const = (T)1;
        
        // ALL AT ONCE (simplify, steal and aggregate constants, inherit operands)
        for (auto it = this->operands.begin(); it != this->operands.end();)
        {
          // correct way to replace element in list
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // if a product is detected, add its operands to the end, then erase it
          if ((*it)->type() == typeid(Mul<T>))
          {
            prod_of_const *= std::dynamic_pointer_cast<ASTOp<T>>(*it)->steal_const(1);
            this->merge_operands(*it);
            it = this->operands.erase(it);
          }
          // if a constant is detected, add its value to the sum, then erase it
          else if ((*it)->type() == typeid(ASTConst<T>))
          {
            prod_of_const *= std::dynamic_pointer_cast<ASTConst<T>>(*it)->value;
            it = this->operands.erase(it);
          }
          else
          {
            ++it;
          }
          if (prod_of_const == 0)
          {
            this->operands.clear();
            break;
          }
        }

        // zero (elements) product
        if (this->operands.size() == 0)
          return std::make_shared<ASTConst<T>>(prod_of_const);

        // add new constant
        if (prod_of_const != 1)
          this->add_constant(std::make_shared<ASTConst<T>>(prod_of_const));
        
        // one-element product
        if (this->operands.size() == 1)
          return this->operands.front();
        
        this->_simplified = true;
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<MulSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<MulSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~Mul() = default;
    };
    
    template <typename T>
    class Eq : public ASTOp<T>
    {
    public:
      Eq(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("==")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        bool all_equal_subexp = true;
        bool first_set = false;
        size_t hash_of_first;

        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // correct way to replace element in list
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // for checking syntactic equality we need to normalize
          (*it)->normalize(true);
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal_subexp = false;
          }
        }
        if (all_equal_subexp)
          return std::make_shared<ASTConst<T>>(1);
        
        this->_simplified = true;
        this->normalize(false); // all sub elements have been already normalized, so we're saving computation
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<EqSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<EqSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~Eq() = default;
    };
    
    template <typename T>
    class Ne : public ASTOp<T>
    {
    public:
      Ne(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("!=")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        bool all_equal_subexp = true;
        bool first_set = false;
        size_t hash_of_first;
        
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // correct way to replace element in list
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // for checking syntactic equality we need to normalize
          (*it)->normalize(true);
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal_subexp = false;
          }
        }
        if (all_equal_subexp)
          return std::make_shared<ASTConst<T>>(0);
        
        this->_simplified = true;
        this->normalize(false); // all sub elements have been already normalized, so we're saving computation
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<NeSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<NeSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~Ne() = default;
    };
    
    template <typename T>
    class Le : public ASTOp<T>
    {
    public:
      Le(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("<=")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        bool all_equal_subexp = true;
        bool first_set = false;
        size_t hash_of_first;
        
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // correct way to replace element in list
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // for checking syntactic equality we need to normalize
          (*it)->normalize(true);
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal_subexp = false;
          }
        }
        if (all_equal_subexp)
          return std::make_shared<ASTConst<T>>(1);
        
        this->_simplified = true;
        this->normalize(false); // all sub elements have been already normalized, so we're saving computation
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<LeSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<LeSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~Le() = default;
    };
    
    template <typename T>
    class Lt : public ASTOp<T>
    {
    public:
      Lt(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("<")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        bool all_equal_subexp = true;
        bool first_set = false;
        size_t hash_of_first;
        
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // correct way to replace element in list
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // for checking syntactic equality we need to normalize
          (*it)->normalize(true);
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal_subexp = false;
          }
        }
        if (all_equal_subexp)
          return std::make_shared<ASTConst<T>>(0);
        
        this->_simplified = true;
        this->normalize(false); // all sub elements have been already normalized, so we're saving computation
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<LtSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<LtSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~Lt() = default;
    };
    
    template <typename T>
    class AllDiff : public ASTOp<T>
    {
    public:
      AllDiff(const std::vector<Exp<T>>& v) : ASTOp<T>("alldifferent")
      {
        for (const Exp<T>& e : v)
          this->append_operand(v);
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        bool all_equal_subexp = true;
        bool first_set = false;
        size_t hash_of_first;
        
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // correct way to replace element in list
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // for checking syntactic equality we need to normalize
          (*it)->normalize(true);
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal_subexp = false;
          }
        }
        if (all_equal_subexp)
          return std::make_shared<ASTConst<T>>(0);
        
        this->_simplified = true;
        this->normalize(false); // all sub elements have been already normalized, so we're saving computation
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<AllDiffSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<AllDiffSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~AllDiff() = default;
    };
    
    template <typename T>
    class Abs : public ASTOp<T>
    {
    public:
      Abs(const Exp<T>& e) : ASTOp<T>("abs")
      {
        this->append_operand(e);
      }
      
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        auto op = *this->operands.begin();
        
        if (!(op->simplified()))
        {
          auto sim = op->simplify();
          if (sim->type() == typeid(ASTConst<T>))
          {
            auto c = dynamic_cast<ASTConst<T>*>(sim.get());
            if (c->value >= 0)
              return sim;
            else
              return std::make_shared<ASTConst<T>>(-c->value);
          }
          if (sim != op)
          {
            this->operands.clear();
            this->append_operand(sim);
            op = sim;
          }
        }
        
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<AbsSym>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<AbsSym<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      virtual ~Abs() = default;
    };
  }
}

#endif // _AST_HH
