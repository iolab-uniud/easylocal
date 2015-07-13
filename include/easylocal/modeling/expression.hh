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

  /** In the EasyLocal::Modeling layer, expressions (Exp) are built from basic
      components (variables and constants) using operator overloading and high
      level constructs inspired by their constraint programming (CP)
      counterparts, e.g., alldifferent and element.
          Behind the scenes, these operators build an abstract syntax tree (AST)
      whose responsibility is to simplify the expressions (if possible) and
      normalize them so that it is easy to implement a hash function and
      recognize existing sub-expressions. Note: mostly, the normalization boils
      down to collapsing and sorting the operands of an Op (operation) node.
      Moreover, the AST keeps track of the depth of each expression, so that the
      bottom-up evaluation (necessary to implement automatic deltas) is as
      efficient as possible, i.e., every time we evaluate a node, its descendants
      have been evaluated already.
   
      FIXME:  some of the equality checks are done using hash comparison. This is
              silly because of how hash functions work. We should consider using
              a perfect hashing function plus look-up table and check for deep
              equality.
   
      FIXME:  indices are always size_t, it doesn't make sense for an index to be
              an Exp<float> or other

    */
  namespace Modeling {
    
    template <typename T>
    class Var;
    
    template <typename T>
    class Const;
    
    template <typename T>
    class Array;

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
      Exp() : _hash_computed(false), _simplified(false), _normalized(false), _const_checked(false) { }

      /** Virtual destructor (for inheritance). */
      virtual ~Exp() = default;

      /** Exposed hash function (caches the computation of the hash).
          @return a unique (integer) identifier for a node.
       */
      size_t hash() const
      {
        if (!_hash_computed)
        {
          this->_hash = compute_hash();
          this->_hash_computed = true;
        }
        return this->_hash;
      }
      
      /** Default normalization function. Makes sure the subtree has a specific ordering. */
      virtual void normalize(bool recursive)
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
      bool simplified() const
      {
        return this->_simplified;
      }

      /** Normalization check.
          @return true if the node does not need to be normalized.
       */
      bool normalized() const
      {
        return this->_normalized;
      }
      
      /** Constness check (for compilation). 
          @return true if expression is const.
       */
      virtual bool is_const() const
      {
        if (!_const_checked)
        {
          this->_const = check_const();
          this->_const_checked = true;
        }
        return this->_const;
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
      const std::type_info& type() const
      {
        return typeid(*this);
      }
      
      /** Explicit cast to Array<T>.
          @return an Array shared pointer to this expression
       */
      std::shared_ptr<Array<T>> as_array()
      {
        return std::static_pointer_cast<Array<T>>(this->shared_from_this());
      }
      
      /** Explicit cast to Matrix<T>.
          @return a Matrix shared pointer to this expression
       */
      std::shared_ptr<Matrix<T>> as_matrix()
      {
        return std::static_pointer_cast<Matrix<T>>(this->shared_from_this());
      }

      /** Explicit cast to Var<T>.
          @return a Var shared pointer to this expression
       */
      std::shared_ptr<Var<T>> as_var()
      {
        return std::static_pointer_cast<Var<T>>(this->shared_from_this());
      }
      
      /** Explicit cast to Const<T>.
          @return a Const shared pointer to this expression
       */
      std::shared_ptr<Const<T>> as_const()
      {
        return std::static_pointer_cast<Const<T>>(this->shared_from_this());
      }
      
      /** Explicit cast to Exp<T>.
          @return a Exp shared pointer to this expression
       */
      std::shared_ptr<Exp<T>> as_exp()
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
      
      /** Check if expression is const.
          @return true if the expression is constant
       */
      virtual bool check_const() const
      {
        return false;
      }
      
      /** Checks whether an Exp has been compiled and added to an
          ExpressionStore, creates it if not.
          @param exp_store ExpressionStore where to find or create a CExp.
          @return a pair containing the CExp index in the expression store and
          a pointer to the CExp itself.
       */
      template <template <typename> class CType>
      std::pair<size_t, std::shared_ptr<CType<T>>> get_or_create(ExpressionStore<T>& exp_store) const
      {
        // If the compiled expression is found in the ExpressionStore, return index and nullptr
        auto it = exp_store.compiled_exps.find(this->hash());
        if (it != exp_store.compiled_exps.end())
          return std::make_pair(it->second, nullptr);

        // Otherwise, generate a new index
        size_t this_index = exp_store.size();

        // Register the hash in the list of compiled expressions
        exp_store.compiled_exps[this->hash()] = this_index;

        // Generate the correct CExp using the template parameter
        std::shared_ptr<CType<T>> compiled = std::make_shared<CType<T>>(exp_store);

        // Push the CExp on the ExpressionStore, update its index
        exp_store.push_back(compiled);
        exp_store[this_index]->index = this_index;

        // Get a string description of the expression
        std::ostringstream os;
        this->Print(os);
        exp_store[this_index]->exp = os.str();

        // Return index and pointer
        return std::make_pair(this_index, compiled);
      }

      /** Keeps track whether the hash has been computed already. */
      mutable bool _hash_computed;

      /** Keep track whether the Exp has been simplified. */
      bool _simplified;
      
      /** Keep track whether the Exp has been simplified. */
      bool _normalized;

      /** Keep track whether the Exp has been simplified. */
      mutable bool _const_checked;
      
      /** Hash code. */
      mutable size_t _hash;
            
      /** Whether expression is const. */
      mutable bool _const;

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
      
      /** @copydoc Exp::normalize(bool). @remarks Does nothing. */
      void normalize(bool recursive) { }

      /** @copydoc Exp::simplify(). @remarks Does nothing. */
      std::shared_ptr<Exp<T>> simplify() { return this->shared_from_this(); }

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
      virtual void Print(std::ostream& os) const
      {
        //os << name << " \u2208 {" << lb << ".." << ub << "}";
        os << name << "{" << lb << ".." << ub << "}";
      }

      /** Virtual destructor. */
      virtual ~Var() = default;

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        return this->template get_or_create<CVar>(exp_store).first;
      }

    protected:

      /** Name of the variable (for printing purposes). */
      const std::string name;

      /** @copydoc Exp::compute_hash() */
      virtual size_t compute_hash() const
      {
        return std::hash<std::string>()(this->name);
      }
      
      /** A variable is not constant. */
      virtual bool check_const() const
      {
        return false;
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
      virtual void Print(std::ostream& os) const
      {
        os << (T)(this->value);
      }

      /** Virtual destructor. */
      virtual ~Const() = default;

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
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
      
      /** @copydoc Exp::check_const() */
      virtual bool check_const() const
      {
        return true;
      }

      /** @copydoc Exp::compute_hash() */
      virtual size_t compute_hash() const
      {
        return std::hash<T>()(this->value);
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
      virtual void Print(std::ostream& os) const
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
      virtual void append_operand(const std::shared_ptr<Exp<T>>& operand)
      {
        this->operands.push_back(operand);
        
        // Adding an operand invalidates the caches
        this->_normalized = false;
        this->_simplified = false;
        this->_hash_computed = false;
        this->_const_checked = false;
      }

      /** Virtual destructor. */
      virtual ~Op() = default;
      
      /** Symbol of the operation (for printing purposes). */
      std::string sym;
      
      /** Operands. */
      std::list<std::shared_ptr<Exp<T>>> operands;

      /** @copydoc Exp::normalize(bool) */
      virtual void normalize(bool recursive)
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
      virtual void normalize_operands(bool recursive)
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
      virtual T steal_const(T def)
      {
        std::shared_ptr<Exp<T>> front = this->operands.front();
        if (front->type() == typeid(Const<T>))
        {
          this->operands.pop_front();
          return std::static_pointer_cast<Const<T>>(front)->value;
        }
        this->_hash_computed = false;
        // this->_normalized = false;     // Removing a constant does not change
        // this->_simplified = false;     // whether the expression is constant
        // this->_const_checked = false;  // normalized, or simplified.
        return def;
      }
      
      /** Adds a constant to the operands. */
      virtual void add_constant(const T& o)
      {
        // Add const operand (at the front of the vector)
        this->operands.push_front(std::make_shared<Const<T>>(o));
        this->_hash_computed = false;
        // this->_normalized = false;     // Adding a constant does not change
        // this->_const_checked = false;  // whether the expression is constant
                                          // or normalized.
        
        this->_simplified = false;        // Might make expression unsimplified
      }
      
    protected:

      /** Adds to itself all the operands from another op. */
      virtual void merge_operands(const std::shared_ptr<Exp<T>>& other)
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
        this->_const_checked = false;
      }

      /** Constructor.
          @param sym symbol of the operation (for printing purposes).
       */
      Op(const std::string& sym) : sym(sym) { }

      /** @copydoc Exp::compute_hash() */
      virtual size_t compute_hash() const
      {
        std::ostringstream os;
        this->Print(os);
        size_t h = std::hash<std::string>()(os.str());
        return h;
      }

      /** Compile each operand, add it to the expression store (recursive, used by compile).
          @param this_index index of the operation in the ExpressionStore
          @param exp_store ExpressionStore where to register the children
       */
      void compile_operands(size_t this_index, ExpressionStore<T>& exp_store) const
      {
        for (auto& op : this->operands)
        {
          size_t child_index = op->compile(exp_store);
          exp_store[this_index]->children.push_back(child_index);
          exp_store[child_index]->parents.insert(this_index);
        }
      }
      
      /** Checks whether this operation has a constant value. 
          @return true if all operands are true
       */
      virtual bool check_const() const
      {
        for (auto& op : this->operands)
          if (!op->is_const())
            return false;
        return true;
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
      virtual void normalize(bool recursive)
      {
        // Normalize operands
        this->normalize_operands(recursive);
        
        // Sort operands
        this->operands.sort([](const std::shared_ptr<Exp<T>>& o1, const std::shared_ptr<Exp<T>>& o2) {
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

      /** @copydoc Exp::simplify() */
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
        this->_const_checked = false;
        
        return this->shared_from_this();
      }

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CSum>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CSum<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      /** Virtual destructor. */
      virtual ~Sum() = default;
    };
    
    /** Element operator, must be defined here in order for Array to be able to
     *  create it.
     */

    template <typename T>
    class Element : public Exp<T>
    {
    public:
      
      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << "element(" << what << "," << where << ")";
      }

      /** No(p)rmalization. */
      void normalize(bool recursive) { }
      
      /** No simplified. */
      std::shared_ptr<Exp<T>> simplify() { return this->shared_from_this(); }
      
      /** Constructor. 
          @param v expression (array)
          @param i expression representing index
       */
      Element(const std::shared_ptr<Exp<T>>& v, const std::shared_ptr<Exp<T>>& i) : const_array(false)
      {
        if (v->type() != typeid(Array<T>) && v->type() != typeid(Matrix<T>))
          throw std::logic_error("Element can be applied only to Arrays and Matrices.");
        
        what = v;
        where = i;
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        throw std::logic_error("Not implemented");
        // FIXME
//        if (!const_array)
//        {
//          auto compiled_pair = this->template get_or_create<CElement>(exp_store);
//          if (compiled_pair.second != nullptr)
//          {
//            auto compiled = std::static_pointer_cast<CElement<T>>(compiled_pair.second);
//            this->compile_operands(compiled_pair.first, exp_store);
//          }
//          return compiled_pair.first;
//        }
//        else
//        {
//          auto compiled_pair = this->template get_or_create<CArrayElement>(exp_store);
//          if (compiled_pair.second != nullptr)
//          {
//            auto compiled = std::static_pointer_cast<CArrayElement<T>>(compiled_pair.second);
//            this->compile_operands(compiled_pair.first, exp_store);
//          }
//          return compiled_pair.first;
//        }
      }
      
      virtual ~Element() = default;
      
      
    protected:

      bool const_array;
      
      std::shared_ptr<Exp<T>> what;
      std::shared_ptr<Exp<T>> where;

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
      Array(const std::string& name, size_t size, const T& lb, const T& ub) : Op<T>("Array"), name(name)
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
        if (name.empty())
          os << this->sym;
        else
          os << this->name;
        os << "[" << this->operands.size() << "]";
        return;
        
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
      
      std::shared_ptr<Exp<T>>& at(const T& i)
      {
        auto it = this->operands.begin();
        std::advance(it, i);
        return *it;
      }
      
        
      std::shared_ptr<Exp<T>> at(const std::shared_ptr<Exp<T>>& i)
      {
        if (i->type() == typeid(Const<T>))
          return this->at(std::static_pointer_cast<Const<T>>(i)->value);
      
        return std::make_shared<Element<T>>(this->shared_from_this(), i);
      }

      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        // Generate CExp (CArray)
        auto compiled_pair = this->template get_or_create<CArray>(exp_store);
        
        // Special handling for CArrays
        if (compiled_pair.second != nullptr)
        {
          // Get newly compiled expression (set start and size if it's a new CArray)
          auto compiled = std::static_pointer_cast<CArray<T>>(compiled_pair.second);
          compiled->size = this->operands.size();
        }
        return compiled_pair.first;
      }
      
    protected:
      
      /** Name of the array (only used for arrays initialized as variable arrays). */
      std::string name;
      
      /** @copydoc Exp::compute_hash() */
      virtual size_t compute_hash() const
      {
        std::ostringstream os;
        os << this->sym;
        for (auto& op : this->operands)
          os << op;
        return std::hash<std::string>()(os.str());
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
        this->_const = true; // optimization
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
      
      std::shared_ptr<Exp<T>>& at(const T& i, const T& j)
      {
        auto it = this->operands.begin();
        std::advance(it, i * cols + j);
        return *it;
      }
      
      
      std::shared_ptr<Exp<T>> at(const std::shared_ptr<Exp<T>>& i, const std::shared_ptr<Exp<T>>& j)
      {
        if (i->type() == typeid(Const<T>) && j->type() == typeid(Const<T>))
          return this->at(std::static_pointer_cast<Const<T>>(i)->value, std::static_pointer_cast<Const<T>>(j)->value);

        auto index = i * (T)(this->cols) + j;
        
        return std::make_shared<Element<T>>(this->shared_from_this(), index);
      }
      
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        // Generate CExp (CArray)
        auto compiled_pair = this->template get_or_create<CArray>(exp_store);
        
        // Special handling for CArrays
        if (compiled_pair.second != nullptr)
        {
          // Get newly compiled expression (set start and size if it's a new CArray)
          auto compiled = std::static_pointer_cast<CArray<T>>(compiled_pair.second);
          compiled->size = this->operands.size();
        }
        return compiled_pair.first;
      }
      
    protected:
      
      std::string name;
      
      size_t rows, cols;
      
      /** @copydoc Exp::compute_hash() */
      virtual size_t compute_hash() const
      {
        std::ostringstream os;
        os << this->sym;
        for (auto& op : this->operands)
          os << op;
        return std::hash<std::string>()(os.str());
      }
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

      /** @copydoc Exp::simplify() */
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

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMul>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CMul<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
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

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CDiv>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CDiv<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
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

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMod>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CMod<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
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

      /** @copydoc Exp::compile() */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMin>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CMin<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
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

      /** @copydoc compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMax>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CMax<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        bool first_set = false;
        size_t hash_of_first;

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
          
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal = false;
          }
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

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CEq>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CEq<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        bool first_set = false;
        size_t hash_of_first;

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
          
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal = false;
          }
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

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CNe>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CNe<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        T last = std::numeric_limits<T>::min();
        bool sorted = true;
        bool first_set = false;
        size_t hash_of_first;
        
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
          
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal = false;
          }
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

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CLe>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CLe<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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

      /** @copydoc Exp::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
      {
        bool all_equal = true;
        bool all_const = true;
        T last = std::numeric_limits<T>::min();
        bool sorted = true;
        bool first_set = false;
        size_t hash_of_first;
        
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
          
          if (!first_set)
          {
            hash_of_first = (*it)->hash();
            first_set = true;
          }
          else
          {
            if (hash_of_first != (*it)->hash())
              all_equal = false;
          }
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

      /** @copydoc Exp::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CLt>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CLt<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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
      NValues(const Array<T>& a) : SymOp<T>("n_values")
      {
        for (const Exp<T>& e : a)
          this->append_operand(e);
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

      /** @copydoc Exp::simplify() */
      virtual std::shared_ptr<Exp<T>> simplify()
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
          return std::make_shared<Const<T>>(1);

        this->_simplified = true;
        this->normalize(false); // all sub elements have been already normalized, so we're saving computation

        return this->shared_from_this();
      }

      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CAllDiff>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CAllDiff<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~NValues() = default;
    };

    template <typename T>
    class Abs : public SymOp<T>
    {
    public:
      Abs(const std::shared_ptr<Exp<T>>& e) : SymOp<T>("abs")
      {
        this->append_operand(e);
      }

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

      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CAbs>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CAbs<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~Abs() = default;
    };

    template <typename T>
    class IfThenElse : public Op<T>
    {
    public:

      IfThenElse(const std::shared_ptr<Exp<T>>& cond, const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2) : Op<T>("if then else")
      {
        this->append_operand(cond);
        this->append_operand(e1);
        this->append_operand(e2);
      }

      virtual std::shared_ptr<Exp<T>> simplify()
      {
        // ALL AT ONCE (simplify, steal and aggregate constants, inherit operands)
        for (auto it = this->operands.begin(); it != this->operands.end(); ++it)
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

      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CIfThenElse>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::static_pointer_cast<CIfThenElse<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
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
