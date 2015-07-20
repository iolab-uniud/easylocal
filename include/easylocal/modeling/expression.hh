#ifndef _EXPRESSION_HH
#define _EXPRESSION_HH

#include <memory>
#include <vector>
#include <list>
#include <map>
#include <iterator>

#include "easylocal/utils/printable.hh"
#include "easylocal/modeling/compiledexpression.hh"

namespace EasyLocal {

  namespace Modeling {
    
    /** Forward declaration. */
    template <typename T>
    class Var;

    /** Forward declaration. */
    template <typename T>
    class Const;
    
    /** Forward declaration. */
    template <typename T>
    class Array;
    
    /** Forward declaration. */
    template <typename T>
    class Matrix;
    
    /** Template class representing a generic node in the AST. To be specialized
     in order to implement specific types of nodes, e.g., vars and constants.
     */
    template <typename T>
    class Exp  : public virtual Core::Printable, public std::enable_shared_from_this<Exp<T>>
    {
    public:
      
      /** Default constructor. */
      Exp() : _hash_computed(false), _simplified(false), _normalized(false) { }
      
      /** Virtual destructor (for inheritance). */
      virtual ~Exp() = default;
      
      /** Exposed hash function (caches the computation of the hash).
       @return a unique (integer) identifier for a node.
       */
      inline size_t hash() const
      {
        if (!_hash_computed)
        {
          this->_hash = compute_hash();
          this->_hash_computed = true;
        }
        return this->_hash;
      }
      
      /** Equality check (internal use, doesn't generate an Eq<T>, cached).
       @param o other expression
       @return true if the two expressions are symbolically equal
       */
      inline bool equals_to(const std::shared_ptr<const Exp<T>>& o) const
      {
        // If type is different, cannot be equal
        if (o->type() != this->type())
          return false;
        
        // If hash is different, cannot be equal
        if (o->hash() != this->hash())
          return false;
        
        return _equals_to(o);
      }
      
      /** Default normalization function. Makes sure the subtree has a specific ordering. */
      virtual void normalize(bool recursive = true)
      {
        throw std::logic_error("Cannot normalize generic Exp<T>.");
      }
      
      /** Default simplify function.
       @return a simplified subtree of the AST.
       */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        throw std::logic_error("Cannot simplify generic Exp<T>.");
      }
      
      /** Simplification check.
       @return true if the node does not need to be simplified.
       */
      inline bool simplified() const
      {
        return this->_simplified;
      }
      
      /** Normalization check.
       @return true if the node does not need to be normalized.
       */
      inline bool normalized() const
      {
        return this->_normalized;
      }
      
      /** "Compiles" the AST node into a CExp.
       @param exp_store ExpressionStore the CExp has to be added to.
       @return the position of the CExp in the ExpressionStore.
       */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        throw std::logic_error("Cannot compile generic Exp<T>.");
      }
      
      /** Type check.
       @return a type descriptor for this expression.
       */
      inline const std::type_info& type() const
      {
        return typeid(*this);
      }
      
      /** Explicit cast to Array<T>.
       @return an Array shared pointer to this expression
       */
      inline std::shared_ptr<Array<T>> as_array()
      {
        return std::static_pointer_cast<Array<T>>(this->shared_from_this());
      }
      
      /** Explicit cast to Matrix<T>.
       @return a Matrix shared pointer to this expression
       */
      inline std::shared_ptr<Matrix<T>> as_matrix()
      {
        return std::static_pointer_cast<Matrix<T>>(this->shared_from_this());
      }
      
      /** Explicit cast to Var<T>.
       @return a Var shared pointer to this expression
       */
      inline std::shared_ptr<Var<T>> as_var()
      {
        return std::static_pointer_cast<Var<T>>(this->shared_from_this());
      }
      
      /** Explicit cast to Const<T>.
       @return a Const shared pointer to this expression
       */
      inline std::shared_ptr<Const<T>> as_const()
      {
        return std::static_pointer_cast<Const<T>>(this->shared_from_this());
      }
      
      /** Explicit cast to Exp<T>.
       @return a Exp shared pointer to this expression
       */
      inline std::shared_ptr<Exp<T>> as_exp()
      {
        return this->shared_from_this();
      }
      
      /** @copydoc Printable::Print(std::ostreamt&) */
      virtual void Print(std::ostream& os = std::cout) const { }
      
    protected:
      
      /** Virtual function to compute the hash.
       @return a number representing the hash of this expression
       */
      virtual size_t compute_hash() const
      {
        throw std::logic_error("Cannot hash generic Exp<T>.");
      }
      
      /** Equality check (internal use, doesn't generate an Eq<T>, cached).
       @param o other expression
       @return true if the two expressions are symbolically equal
       */
      virtual bool _equals_to(const std::shared_ptr<const Exp<T>>& o) const
      {
        throw std::logic_error("Cannot compare generic Exp<T>.");
      }
      
      /** Checks whether an Exp has been compiled and added to an ExpressionStore, creates it if not.
          @param exp_store ExpressionStore where to find or create a CExp.
          @return a pair containing the CExp index in the expression store and a pointer to the CExp itself.
       */
      template <template <typename> class CType>
      std::pair<size_t, std::shared_ptr<CType<T>>> get_or_create(ExpressionStore<T>& exp_store) const
      {
        // Get own shared_ptr
        auto s_this = this->shared_from_this();
        
        // Check whether the expression has been compiled already
        if (exp_store.contains(s_this))
          return std::make_pair(exp_store.index_of(s_this), nullptr);
        
        // Otherwise compile correct CExp accorgin to template type
        auto compiled = std::make_shared<CType<T>>(exp_store);

        // Register expression and get index back
        size_t i = exp_store.register_as(std::static_pointer_cast<CExp<T>>(compiled), s_this);
        
        // Return index and pointer
        return std::make_pair(i, compiled);
      }
      
      /** Keeps track whether the hash has been computed already. */
      mutable bool _hash_computed;
      
      /** Keep track whether the Exp has been simplified. */
      bool _simplified;
      
      /** Keep track whether the Exp has been simplified. */
      bool _normalized;
      
      /** Hash code. */
      mutable size_t _hash;
      
    };
    
    /** Special Exp which does not need to be simplified or normalized. Note
     *  that this doesn't mean the expression is constant.
     */
    template <typename T>
    class StableExp : public Exp<T>
    {
    public:
      
      /** Virtual destructor. */
      virtual ~StableExp() = default;
      
      /** @copydoc Exp<T>::normalize(bool). @remarks Does nothing. */
      virtual void normalize(bool recursive) { }
      
      /** @copydoc Exp<T>::simplify(). @remarks Does nothing. */
      virtual inline std::shared_ptr<Exp<T>> simplify() { return this->shared_from_this(); }
      
    protected:
      
      /** Default constructor. */
      StableExp()
      {
        this->_simplified = true;
        this->_normalized = true;
      }
    };
    
    /** An Exp representing a variable. */
    template <typename T>
    class Var : public StableExp<T>
    {
    public:
      
      /** Constructor.
       @param name name of the variable (for printing purposes).
       */
      Var(const std::string& name, const T& lb, const T& ub) : name(name), lb(lb), ub(ub) { }
      
      /** @copydoc Printable::Print(std::ostream& os) */
      inline virtual void Print(std::ostream& os) const
      {
        //os << name << " \u2208 {" << lb << ".." << ub << "}";
        os << name << "{" << lb << ".." << ub << "}";
      }
      
      /** Virtual destructor. */
      virtual ~Var() = default;
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        return this->template get_or_create<CVar>(exp_store).first;
      }
      
    protected:
      
      /** @copydoc Exp<T>::_equals_to(const std::shared_ptr<Exp<T>>&) */
      inline virtual bool _equals_to(const std::shared_ptr<const Exp<T>>& o) const
      {
        const auto& co = std::static_pointer_cast<const Var<T>>(o);
        if (name != co->name)
          return false;
        if (lb != co->lb || ub != co->ub)
          return false;
        return true;
      }
      
      /** Name of the variable (for printing purposes). */
      const std::string name;
      
      /** @copydoc Exp<T>::compute_hash() */
      inline virtual size_t compute_hash() const
      {
        return 31 * typeid(*this).hash_code() + std::hash<std::string>()(this->name);
      }
      
      /** Lower and upper bounds. */
      T lb, ub;
      
    };
    
    /** Exp representing constant value. */
    template <typename T>
    class Const : public StableExp<T>
    {
    public:
      
      /** Constructor.
       @param value value of the constant.
       */
      Const(const T& value) : value(value) { }
      
      /** @copydoc Printable::Print(std::ostream& os) */
      inline virtual void Print(std::ostream& os) const
      {
        os << (T)(this->value);
      }
      
      /** Virtual destructor. */
      virtual ~Const() = default;
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CConst>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CConst<T>>(compiled_pair.second);
          compiled->value = this->value;
        }
        return compiled_pair.first;
      }
      
      /** Value of the constant (safe to be public since it's constant). */
      const T value;
      
    protected:
      
      /** @copydoc Exp<T>::_equals_to(const std::shared_ptr<Exp<T>>&) */
      inline virtual bool _equals_to(const std::shared_ptr<const Exp<T>>& o) const
      {
        const auto& co = std::static_pointer_cast<const Const<T>>(o);
        if (value != co->value)
          return false;
        return true;
      }
      
      /** @copydoc Exp<T>::compute_hash() */
      inline virtual size_t compute_hash() const
      {
        return 31 * typeid(*this).hash_code() + std::hash<T>()(this->value);
      }
      
    };
    
    /** Generic class representing an operation. To be specialized to implement
     specific operators.
     */
    template <typename T>
    class Op : public Exp<T>
    {
    public:
      
      /** @copydoc Printable::Print(std::ostream& os) */
      inline virtual void Print(std::ostream& os) const
      {
        if (operands.size() == 1)
          os << this->sym;
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
      
      /** Append an Exp<T> as operand.
       @param operand Exp<T> to add.
       */
      inline void append_operand(const std::shared_ptr<Exp<T>>& operand)
      {
        this->operands.push_back(operand);
        
        // Adding an operand invalidates the caches
        this->_normalized = false;
        this->_simplified = false;
        this->_hash_computed = false;
      }
      
      /** Virtual destructor. */
      virtual ~Op() = default;
      
      /** Symbol of the operation (for printing purposes). */
      std::string sym;
      
      /** Operands. */
      std::list<std::shared_ptr<Exp<T>>> operands;
      
      /** @copydoc Exp<T>::normalize(bool) */
      inline virtual void normalize(bool recursive)
      {
        // Normalize operands
        this->normalize_operands(recursive);
        
        // Do nothing (for non symmetric operations)
        
        // Finally expression is normalized
        this->_normalized = true;
      }
      
      /** Normalize (ensures all operands are normalized).
       @param recursive whether to forward the normalization to operands.
       */
      inline void normalize_operands(bool recursive)
      {
        if (this->normalized())
          return;
        if (recursive)
          for (auto& op : this->operands)
            op->normalize(recursive);
      }
      
      /** Gets constant from operands.
       @param def default value (to be returned if first operand is not const).
       @return the value of the first operand (if const) or the default value.
       */
      inline T steal_const(T def)
      {
        std::shared_ptr<Exp<T>> front = this->operands.front();
        if (front->type() == typeid(Const<T>))
        {
          this->operands.pop_front();
          return std::static_pointer_cast<Const<T>>(front)->value;
        }
        this->_hash_computed = false;
        // this->_normalized = false;     // Removing a constant does not change
        // this->_simplified = false;     // Removing a constant does not change
        return def;
      }
      
      /** Adds a constant to the operands. */
      inline void add_constant(const T& o)
      {
        // Add const operand (at the front of the vector)
        this->operands.push_front(std::make_shared<Const<T>>(o));
        this->_hash_computed = false;
        // this->_normalized = false;     // Adding a constant does not change
        this->_simplified = false;        // Adding a constant might change
      }
      
    protected:
      
      /** @copydoc Exp<T>::_equals_to(const std::shared_ptr<Exp<T>>&) */
      inline virtual bool _equals_to(const std::shared_ptr<const Exp<T>>& o) const
      {
        const auto& co = std::static_pointer_cast<const Op<T>>(o);
        
        // Operation type has been checked already, we need to check for equality of operands (order must be the same also)
        if (this->operands.size() != co->operands.size())
          return false;
        
        bool all_equals = true;
        auto top = this->operands.begin();
        auto oop = co->operands.begin();
        for (; top != this->operands.end() ; top++)
        {
          if (!(*top)->equals_to(*oop))
          {
            all_equals = false;
            break;
          }
          oop++;
        }
        
        return all_equals;
      }
      
      /** Adds to itself all the operands from another op. */
      inline void merge_operands(const std::shared_ptr<Exp<T>>& other)
      {
        // Check if other Exp is operation
        Op<T>* p_other = dynamic_cast<Op<T>*>(other.get());
        if (p_other == nullptr)
          return;
        
        // Move operands to other's operands (generally faster because of how splice is implemented)
        p_other->operands.splice(p_other->operands.end(), this->operands);
        
        // Swap operands
        std::swap(this->operands, p_other->operands);
        
        // Invalidate caches
        this->_normalized = false;
        this->_simplified = false;
        this->_hash_computed = false;
      }
      
      /** Constructor.
       @param sym symbol of the operation (for printing purposes).
       */
      Op(const std::string& sym) : sym(sym) { }
      
      /** @copydoc Exp<T>::compute_hash() */
      virtual size_t compute_hash() const
      {
        int h = typeid(*this).hash_code();
        for (const auto& o : this->operands)
          h = 31 * h + o->hash();
        return h;
      }
      
      /** Compile each operand, add it to the expression store (recursive, used by compile).
       @param this_index index of the operation in the ExpressionStore
       @param exp_store ExpressionStore where to register the children
       */
      inline void compile_operands(size_t this_index, ExpressionStore<T>& exp_store) const
      {
        for (auto& op : this->operands)
        {
          size_t child_index = op->compile(exp_store);
          exp_store[this_index]->children.push_back(child_index);
          exp_store[child_index]->parents.insert(this_index);
        }
      }
    };
    
    /** Generic class for symmetric operations. */
    template <typename T>
    class SymOp : public Op<T>
    {
    public:
      
      /** Constructor is the same as Op. */
      using Op<T>::Op;
      
      /** Normalize. Sorts the operands (for hashing purposes).
       @param recursive whether it should be called recursively.
       */
      inline virtual void normalize(bool recursive)
      {
        // Normalize operands
        this->normalize_operands(recursive);
        
        // Sort operands
        this->operands.sort([](const std::shared_ptr<Exp<T>>& o1, const std::shared_ptr<Exp<T>>& o2) {
          
          // Consts must come first in the ordering (to allow for a sound steal_const implementation)
          if (o1->type() == typeid(Const<T>) && o2->type() != typeid(Const<T>))
            return true;
          
          // Enforce a specific ordering of types, establish hash-based ordering within objects of the same type
          return (o1->type().hash_code() < o2->type().hash_code()) || (o1->type().hash_code() == o2->type().hash_code() && o1->hash() < o2->hash());
          
        });
        
        // Finally expression is normalized
        this->_normalized = true;
      }
    };
    
    /** Sum operation. */
    template <typename T>
    class Sum : public SymOp<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Sum(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : SymOp<T>("+")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        T sum_of_const = (T)0;
        
        /** Scan operands, replace each operand with its simplified version,
         if operand is a sum replace it with its operands, ensuring that
         any const is added to the sum_of_conts (so that it ends up being
         a single constant in the simplified operation.
         */
        for (auto it = this->operands.begin(); it != this->operands.end();)
        {
          // Correct way to replace in list (doable because it's a doubly linked list)
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // If a sum is detected, add its operands to the end, then erase it
          if ((*it)->type() == typeid(Sum<T>))
          {
            // Handle constants separately
            sum_of_const += std::static_pointer_cast<Op<T>>(*it)->steal_const(0);
            
            // Steal operands
            this->merge_operands(*it);
            
            // Erase sum
            it = this->operands.erase(it);
          }
          
          // If a constant is detected, add it to the sum_of_const
          else if ((*it)->type() == typeid(Const<T>))
          {
            sum_of_const += std::static_pointer_cast<Const<T>>(*it)->value;
            it = this->operands.erase(it);
          }
          else
          {
            // Normalize operand
            (*it)->normalize(true);
            ++it;
          }
        }
        
        // Add constant
        if (sum_of_const != 0)
          this->add_constant(sum_of_const);
        
        // If the sum has no elements (because of constant elimination), return zero
        if (this->operands.size() == 0)
          return std::make_shared<Const<T>>(0);
        
        // If the sum has only one element, return it
        if (this->operands.size() == 1)
          return this->operands.front();
        
        // Finalize
        this->_simplified = true;
        this->normalize(false);
        
        // Invalidate caches
        this->_hash_computed = false;
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CSum>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Sum() = default;
    };
    
    /** Element operator, must be defined here in order for Array to be able to
     *  create it.
     */
    
    template <typename T>
    class Element : public Op<T>
    {
    public:
      
      /** No(p)rmalization. */
      inline virtual void normalize(bool recursive) { }
      
      /** No simplified. */
      inline std::shared_ptr<Exp<T>> simplify() {
        return this->shared_from_this();
      }
      
      /** Constructor.
       @param v expression (array)
       @param i expression representing index
       */
      Element(const std::shared_ptr<Exp<T>>& i, const std::shared_ptr<Array<T>>& v) : Op<T>("element")
      {
        this->append_operand(i);
        this->append_operand(v);
      }
      
      /** Constructor.
       @param v expression (array)
       @param i expression representing index
       */
      Element(const std::shared_ptr<Exp<T>>& i, const std::shared_ptr<Matrix<T>>& m) : Op<T>("element")
      {
        this->append_operand(i);
        this->append_operand(m);
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CElement>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      virtual ~Element() = default;
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << this->sym;
        os << "(";
        int count = 0;
        for (auto& op : this->operands)
        {
          count++;
          op->Print(os);
          if (count < this->operands.size())
            os << ",";
        }
        os << ")";
      }
      
    };
    
    /** An Exp representing a variable array. */
    template <typename T>
    class Array : public Op<T>
    {
    public:
      
      /** Constructor. */
      Array() { }
      
      /** Constructor with vector of constants. */
      Array(const std::vector<T>& constants) : Op<T>("Array")
      {
        for (auto& c : constants)
          this->append_operand(std::make_shared<Const<T>>(c));
      }
      
      /** Constructor for array of variables. */
      Array(const std::string& name, size_t size, const T& lb, const T& ub) : Op<T>(name)
      {
        for (size_t i = 0; i < size; i++)
        {
          std::ostringstream os;
          os << name << "_" << i;
          this->append_operand(std::make_shared<Var<T>>(os.str(), lb, ub));
        }
      }
      
      /** Constructor with vector of expressions. */
      Array(const std::vector<std::shared_ptr<Exp<T>>>& expressions) : Op<T>("Array")
      {
        this->operands.insert(this->operands.end(), expressions.begin(), expressions.end());
      }
      
      /** Virtual destructor. */
      virtual ~Array() = default;
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << this->sym << "[" << this->operands.size() << "]";
        return;
        
        os << this->sym << "[";
        int count = 0;
        for (auto& op : this->operands)
        {
          count++;
          os << op;
          if (count < this->operands.size())
            os << ",";
        }
        os << "]";
      }
      
      /** Subscript operator.
       @param i index of the expression to retrieve
       @return the expression at index i
       */
      inline std::shared_ptr<Exp<T>>& at(const T& i)
      {
        auto it = this->operands.begin();
        std::advance(it, i);
        return *it;
      }
      
      /** Subscript operator.
       @param i symbolic index of the expression to retrieve
       @return an element operation
       */
      inline std::shared_ptr<Exp<T>> at(const std::shared_ptr<Exp<T>>& i)
      {
        if (i->type() == typeid(Const<T>))
          return this->at(std::static_pointer_cast<Const<T>>(i)->value);
        
        return std::make_shared<Element<T>>(i, this->as_array());
      }
        
      /** Subscript operator.
       @param i index of the expression to retrieve
       @return the expression at index i
       */
      inline const std::shared_ptr<Exp<T>>& operator[](const size_t& i) const
      {
          auto it = this->operands.begin();
          std::advance(it, i);
          return *it;
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CArray>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }

      
    };
    
    /** An Exp representing a variable array. */
    template <typename T>
    class Matrix : public Op<T>
    {
    public:
      
      /** Constructor. */
      Matrix() { }
      
      /** Constructor with vector of constants. */
      Matrix(size_t rows, size_t cols, const std::vector<T>& constants) : Op<T>("Matrix"), rows(rows), cols(cols)
      {
        for (auto& c : constants)
          this->append_operand(std::make_shared<Const<T>>(c));
      }
      
      /** Constructor for array of variables. */
      Matrix(const std::string& name, size_t rows, size_t cols, const T& lb, const T& ub) : Op<T>("Matrix"), name(name), rows(rows), cols(cols)
      {
        for (size_t i = 0; i < rows; i++)
        {
          for (size_t j = 0; j < cols; j++)
          {
            std::ostringstream os;
            os << name << "_" << i << "," << j;
            this->append_operand(std::make_shared<Var<T>>(os.str(), lb, ub));
          }
        }
      }
      
      /** Constructor with vector of expressions. */
      Matrix(size_t rows, size_t cols, const std::vector<std::shared_ptr<Exp<T>>>& expressions) : Op<T>("Matrix"), rows(rows), cols(cols)
      {
        this->operands.insert(this->operands.end(), expressions.begin(), expressions.end());
      }
      
      /** Virtual destructor. */
      virtual ~Matrix() = default;
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        if (name.empty())
          os << this->sym;
        else
          os << this->name;
        os << "[" << rows << ", " << cols << "]";
        return;
        
        // FIXME: print this in a better way
        if (name.empty())
          os << this->sym;
        else
          os << this->name;
        os << "[";
        int count = 0;
        for (auto& op : this->operands)
        {
          count++;
          os << op;
          if (count < this->operands.size())
            os << ",";
        }
        os << "]";
      }
      
      inline std::shared_ptr<Exp<T>>& at(const T& i, const T& j)
      {
        auto it = this->operands.begin();
        std::advance(it, i * cols + j);
        return *it;
      }
      
      
      inline std::shared_ptr<Exp<T>> at(const std::shared_ptr<Exp<T>>& i, const std::shared_ptr<Exp<T>>& j)
      {
        if (i->type() == typeid(Const<T>) && j->type() == typeid(Const<T>))
          return this->at(std::static_pointer_cast<Const<T>>(i)->value, std::static_pointer_cast<Const<T>>(j)->value);
        
        auto ij = i * (T)(this->cols) + j;
        
        return std::make_shared<Element<T>>(ij, this->as_matrix());
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CArray>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
    protected:
      
      std::string name;
      size_t rows, cols;
      
    };
    
    /** Product operation. */
    template <typename T>
    class Mul : public SymOp<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Mul(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : SymOp<T>("*")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        T prod_of_const = (T)1;
        
        /** Scan operands, replace each operand with its simplified version,
         if an operand is a product, steal its operands ensuring that
         every const is added to the prod_of_const, then erase it.
         */
        for (auto it = this->operands.begin(); it != this->operands.end();)
        {
          // Correct way to replace in list (doable because it's a doubly linked list)
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // If operand is a product, steal its operands
          if ((*it)->type() == typeid(Mul<T>))
          {
            // Handle consts properly
            prod_of_const *= std::static_pointer_cast<Op<T>>(*it)->steal_const(1);
            this->merge_operands(*it);
            it = this->operands.erase(it);
          }
          
          // Handle consts
          else if ((*it)->type() == typeid(Const<T>))
          {
            prod_of_const *= std::static_pointer_cast<Const<T>>(*it)->value;
            it = this->operands.erase(it);
          }
          else
          {
            // Normalize operand
            (*it)->normalize(true);
            ++it;
          }
          
          // If product of consts erases, remove all operands
          if (prod_of_const == 0)
          {
            this->operands.clear();
            break;
          }
        }
        
        // Zero (elements) product
        if (this->operands.size() == 0)
          return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(prod_of_const));
        
        // Add new constant
        if (prod_of_const != 1)
          this->add_constant(prod_of_const);
        
        // One-element product
        if (this->operands.size() == 1)
          return this->operands.front();
        
        // Finalize
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMul>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Mul() = default;
    };
    
    /** Division operator. */
    template <typename T>
    class Div : public Op<T> // not symmetric (cannot normalize by sorting the operands)
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Div(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : Op<T>("/")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        std::vector<T> values;
        
        // Scan operands, replace them with simplified version, add consts to values
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // Handle consts
          if ((*it)->type() == typeid(Const<T>))
            values.push_back(std::static_pointer_cast<Const<T>>(*it)->value);
          (*it)->normalize(true);
        }
        
        // If both operands are constants, replace with result
        if (values.size() == 2)
          return std::make_shared<Const<T>>(values[0] / values[1]);
        
        // Finalize
        this->_simplified = true;
        this->_normalized = true;
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CDiv>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Div() = default;
    };
    
    /** Modulo operation. */
    template <typename T>
    class Mod : public Op<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Mod(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : Op<T>("%")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        std::vector<T> values;
        
        /** Scan operands, replace each operand with its simplified version,
         collect consts in values.
         */
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // Handle consts
          if ((*it)->type() == typeid(Const<T>))
            values.push_back(std::static_pointer_cast<Const<T>>(*it)->value);
          (*it)->normalize(true);
        }
        
        // If both operands are constants, replace operation with result
        if (values.size() == 2)
          return std::make_shared<Const<T>>(values[0] % values[1]);
        
        // Finalize
        this->_simplified = true;
        this->_normalized = true;
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMod>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Mod() = default;
    };
    
    /** Minimum between elements. */
    template <typename T>
    class Min : public SymOp<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Min(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : SymOp<T>("min")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        T min_of_const;
        bool min_of_const_set = false;
        
        /** Scan operands, replace each operand with its simplified version,
         handle operands of type Min by merging their operands, handle
         constants separately.
         */
        for (auto it = this->operands.begin(); it != this->operands.end();)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // If a min is detected, add its operands to the end, then erase it
          if ((*it)->type() == typeid(Min<T>))
          {
            // Note: it's not important that we use the std::numeric_limits<T>::min(), as long as it's a "unique" value
            T stolen_const = std::static_pointer_cast<Op<T>>(*it)->steal_const(std::numeric_limits<T>::max());
            
            // Only if a constant was stolen
            if (stolen_const != std::numeric_limits<T>::max())
            {
              // Handle undefined min
              if (!min_of_const_set)
              {
                min_of_const = stolen_const;
                min_of_const_set = true;
              }
              else
                std::min(min_of_const, stolen_const);
            }
            
            this->merge_operands(*it);
            it = this->operands.erase(it);
          }
          
          // If a constant is detected, check it straight away
          else if ((*it)->type() == typeid(Const<T>))
          {
            if (!min_of_const_set)
            {
              min_of_const = std::static_pointer_cast<Const<T>>(*it)->value;
              min_of_const_set = true;
            }
            else
              min_of_const = std::min(min_of_const, std::static_pointer_cast<Const<T>>(*it)->value);
            
            it = this->operands.erase(it);
          }
          
          // If a different operation is detected
          else
          {
            (*it)->normalize(true);
            ++it;
          }
        }
        
        // Add constant to the operands (if necessary)
        if (min_of_const_set)
          this->add_constant(min_of_const);
        
        // If we have only one operand, it must be the minimum
        if (this->operands.size() == 1)
          return this->operands.front();
        
        // Finalize
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << this->sym;
        os << "(";
        int count = 0;
        for (auto& op : this->operands)
        {
          count++;
          op->Print(os);
          if (count < this->operands.size())
            os << ",";
        }
        os << ")";
      }
      
      /** @copydoc Exp<T>::compile() */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMin>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Min() = default;
    };
    
    /** Maximum between elements. */
    template <typename T>
    class Max : public SymOp<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Max(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : SymOp<T>("max")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        T max_of_const;
        bool max_of_const_set = false;
        
        /** Scan operands, replace each operand with its simplified version,
         handle operands of type Max by merging their operands, handle
         constants separately.
         */
        for (auto it = this->operands.begin(); it != this->operands.end();)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // If a min is detected, add its operands to the end, then erase it
          if ((*it)->type() == typeid(Max<T>))
          {
            // Note: it's not important that we use the std::numeric_limits<T>::min(), as long as it's a "unique" value
            T stolen_const = std::static_pointer_cast<Op<T>>(*it)->steal_const(std::numeric_limits<T>::min());
            
            // Only if a constant was stolen
            if (stolen_const != std::numeric_limits<T>::min())
            {
              // Handle undefined min
              if (!max_of_const_set)
              {
                max_of_const = stolen_const;
                max_of_const_set = true;
              }
              else
                std::max(max_of_const, stolen_const);
            }
            
            this->merge_operands(*it);
            it = this->operands.erase(it);
          }
          
          // If a constant is detected, check it straight away
          else if ((*it)->type() == typeid(Const<T>))
          {
            if (!max_of_const_set)
            {
              max_of_const = std::static_pointer_cast<Const<T>>(*it)->value;
              max_of_const_set = true;
            }
            else
              max_of_const = std::max(max_of_const, std::static_pointer_cast<Const<T>>(*it)->value);
            
            it = this->operands.erase(it);
          }
          
          // If a different operation is detected
          else
          {
            (*it)->normalize(true);
            ++it;
          }
        }
        
        // Add constant to the operands (if necessary)
        if (max_of_const_set)
          this->add_constant(max_of_const);
        
        // If we have only one operand, it must be the minimum
        if (this->operands.size() == 1)
          return this->operands.front();
        
        // Finalize
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMax>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Max() = default;
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << this->sym;
        os << "(";
        int count = 0;
        for (auto& op : this->operands)
        {
          count++;
          op->Print(os);
          if (count < this->operands.size())
            os << ",";
        }
        os << ")";
      }
    };
    
    /** Equality operator. */
    template <typename T>
    class Eq : public SymOp<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Eq(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : SymOp<T>("==")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        const std::shared_ptr<Exp<T>>& first = *(this->operands.begin());
        
        /** Scan operands, replace each operand with its simplified version,
         normalize operands so they can be compared using the hash.
         */
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          /** We normalize each oeprand, so we can recognize equal expressions
           and make this operation a constant.
           */
          (*it)->normalize(true);
          
          // Check constness
          all_const = (all_const && ((*it)->type() == typeid(Const<T>)));
          
          // Fail equality test if at least one element is not equals
          if (!first->equals_to(*it))
            all_equal = false;
          
        }
        
        // Return "true" if all the elements are equal
        if (all_equal)
          return std::make_shared<Const<T>>(1);
        
        if (all_const)
          return std::make_shared<Const<T>>(0);
        
        // Finalize (non recursive because we have already normalized)
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CEq>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Eq() = default;
    };
    
    /** Non-equality operator. */
    template <typename T>
    class Ne : public SymOp<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Ne(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : SymOp<T>("!=")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        const std::shared_ptr<Exp<T>>& first = *(this->operands.begin());
        
        /** Scan operands, replace each opeand with its simplified version,
         normalize operands so they can be compared using the hash.
         */
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // Correct way to replace element in list (doable since it's a doubly linked list)
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          // Normalize so we can compare using hash value
          (*it)->normalize(true);
          
          // Check constness
          all_const = (all_const && ((*it)->type() == typeid(Const<T>)));
          
          // Fail constness check if at least one
          if (!first->equals_to(*it))
            all_equal = false;
          
        }
        
        // Return "false" only if the elements are all equal
        if (all_equal)
          return std::make_shared<Const<T>>(0);
        
        if (all_const)
          return std::make_shared<Const<T>>(1);
        
        // Finalize (non recursive because we have already normalized)
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CNe>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Ne() = default;
    };
    
    /** Less-or-equal operator. */
    template <typename T>
    class Le : public Op<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Le(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : Op<T>("<=")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        T last = std::numeric_limits<T>::min();
        bool sorted = true;
        const std::shared_ptr<Exp<T>>& first = *(this->operands.begin());
        
        /** Scan operands, replace each operand with its simplified version,
         normalize operands so they can be compared using the hash.
         */
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          /** We normalize each operand, so we can recognize equal expressions
           and make this operation a constant.
           */
          (*it)->normalize(true);
          
          // Check constness
          all_const = (all_const && ((*it)->type() == typeid(Const<T>)));
          
          if (all_const)
          {
            T curr = std::static_pointer_cast<Const<T>>(*it)->value;
            sorted = (sorted && (curr >= last));
            last = curr;
          }
          
          // Fail constness check if at least one operand is different
          if (!first->equals_to(*it))
            all_equal = false;
          
        }
        
        // Return "true" if all the elements are equal
        if (all_equal)
          return std::make_shared<Const<T>>(1);
        
        if (all_const)
          return std::make_shared<Const<T>>((T)sorted);
        
        // Finalize (non recursive because we have already normalized)
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CLe>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Le() = default;
    };
    
    
    /** Less-than operator. */
    template <typename T>
    class Lt : public Op<T>
    {
    public:
      
      /** Constructor.
       @param e1 first operand
       @param e2 second operand
       */
      Lt(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : Op<T>("<")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        T last = std::numeric_limits<T>::min();
        bool sorted = true;
        const std::shared_ptr<Exp<T>>& first = *(this->operands.begin());
        
        /** Scan operands, replace each operand with its simplified version,
         normalize operands so they can be compared using the hash.
         */
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          /** We normalize each operand, so we can recognize equal expressions
           and make this operation a constant.
           */
          (*it)->normalize(true);
          
          // Check constness
          all_const = (all_const && ((*it)->type() == typeid(Const<T>)));
          
          if (all_const)
          {
            T curr = std::static_pointer_cast<Const<T>>(*it)->value;
            sorted = (sorted && (curr > last));
            last = curr;
          }
          
          // Fail constness check ...
          if (!first->equals_to(*it))
            all_equal = false;
        }
        
        // Return "true" if all the elements are equal
        if (all_equal)
          return std::make_shared<Const<T>>(0);
        
        if (all_const)
          return std::make_shared<Const<T>>((T)sorted);
        
        // Finalize (non recursive because we have already normalized)
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CLt>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Lt() = default;
    };
    
    template <typename T>
    class NValues : public SymOp<T>
    {
    public:
      
      /** Constructor.
       @param a array of values
       */
      NValues(const std::shared_ptr<Array<T>>& a) : SymOp<T>("n_values")
      {
        // Steal operands from array
        for (auto& el : a->operands)
          this->append_operand(el);
      }
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << this->sym;
        os << "(";
        int count = 0;
        for (auto& op : this->operands)
        {
          count++;
          op->Print(os);
          if (count < this->operands.size())
            os << ",";
        }
        os << ")";
      }
      
      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        std::unordered_set<T> unique;
        bool all_const = true;
        
        /** Scan operands, replace each operand with its simplified version,
         normalize operands so they can be compared using the hash.
         */
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          // Correct way to replace element in list (doable because it's a doubly linked list)
          if (!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          
          /** We normalize each operand, so we can recognize equal expressions
           and make this operation a constant.
           */
          (*it)->normalize(true);
          
          // Check constness
          all_const = (all_const && ((*it)->type() == typeid(Const<T>)));
          
          if (all_const)
            unique.insert(std::static_pointer_cast<Const<T>>(*it)->value);
        }
        
        if (all_const)
          return std::make_shared<Const<T>>(unique.size());
        
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CNValues>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      virtual ~NValues() = default;
    };
    
    template <typename T>
    class Abs : public SymOp<T>
    {
    public:
      
      /** Constructor. */
      Abs(const std::shared_ptr<Exp<T>>& e) : SymOp<T>("abs")
      {
        this->append_operand(e);
      }

      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        auto op = *this->operands.begin();
        
        if (!(op->simplified()))
        {
          auto sim = op->simplify();
          if (sim != op)
          {
            this->operands.clear();
            this->append_operand(sim);
            op = sim;
          }
        }
        
        if (op->type() == typeid(Const<T>))
        {
          auto c = static_cast<Const<T>*>(op.get());
          if (c->value >= 0)
            return op;
          else
            return std::make_shared<Const<T>>(-c->value);
        }
        
        this->_simplified = true;
        this->normalize(true);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CAbs>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      virtual ~Abs() = default;
    };
    
    template <typename T>
    class IfThenElse : public Op<T>
    {
    public:
      
      /** Constructor. */
      IfThenElse(const std::shared_ptr<Exp<T>>& cond, const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : Op<T>("if then else")
      {
        this->append_operand(cond);
        this->append_operand(e1);
        this->append_operand(e2);
      }

      /** @copydoc Exp<T>::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
        {
          if(!((*it)->simplified()))
          {
            auto sim = (*it)->simplify();
            
            if (sim != *it)
            {
              it = this->operands.erase(it);
              it = this->operands.insert(it, sim);
            }
          }
          (*it)->normalize(true);
        }
        
        if (this->operands.front()->type() == typeid(Const<T>))
        {
          if (static_cast<Const<T>*>(this->operands.front().get())->value)
            return *std::next(this->operands.begin());
          else
            return *std::next(std::next(this->operands.begin()));
        }
        
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }
      
      /** @copydoc Exp<T>::compile(ExpressionStore<T>&) */
      inline virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CIfThenElse>(exp_store);
        if (compiled_pair.second != nullptr)
          this->compile_operands(compiled_pair.first, exp_store);
        return compiled_pair.first;
      }
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        std::vector<std::shared_ptr<Exp<T>>> ops;
        ops.insert(ops.begin(), this->operands.begin(), this->operands.end());
        os << "if ( " << ops[0] << " ) { " << ops[1] << " } else { " << ops[2] << " }";
      }
      
      virtual ~IfThenElse() = default;
    };
    
  }
}

#endif // _EXPRESSION_HH
