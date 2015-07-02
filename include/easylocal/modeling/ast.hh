#ifndef _AST_HH
#define _AST_HH

#include "easylocal/utils/printable.hh"
#include <memory>
#include <vector>
#include <list>
#include <map>
#include "compiledexpression.hh"

namespace EasyLocal {

  /** In the EasyLocal::Modeling layer, expressions (Exp) are built from basic
      components (variables and constants) using operator overloading and high
      level constructs inspired by their constraint programming (CP)
      counterparts, e.g., alldifferent and element.
          Behind the scenes, these operators build an abstract syntax tree (AST)
      whose responsibility is to simplify the expressions (if possible) and
      normalize them so that it is easy to implement a hash function and
      recognize existing sub-expressions. Note: mostly, the normalization boils
      down to collapsing and sorting the operands of an ASTOp (operation) node.
      Moreover, the AST keeps track of the depth of each expression, so that the
      bottom-up evaluation (necessary to implement automatic deltas) is as
      efficient as possible, i.e., every time we evaluate a node, its descendants
      have been evaluated already.
    */
  namespace Modeling {

    /** Forward declaration. */
    template <typename T>
    class ASTVarArray;

    /** Forward declaration (AST operation). */
    template <typename T>
    class ASTOp;

    /** Forward declaration (AST symmetric operation). */
    template <typename T>
    class ASTSymOp;

    /** Template class representing a generic node in the AST. To be specialized
        in order to implement specific types of nodes, e.g., vars and constants.
     */
    template <typename T>
    class ASTItem : public virtual Core::Printable, public std::enable_shared_from_this<ASTItem<T>>
    {
      /** Needed to access type() which is protected. */
      friend class ASTOp<T>;

      /** Needed to access type() which is protected. */
      friend class ASTSymOp<T>;

    public:

      /** Default constructor. */
      ASTItem() : _hash_set(false), _simplified(false), _normalized(false) { }

      /** Virtual destructor (for inheritance). */
      virtual ~ASTItem() = default;

      /** Exposed hash function (caches the computation of the hash).
          @return a unique (integer) identifier for a node.
       */
      size_t hash() const
      {
        if (!_hash_set)
        {
          this->_hash = compute_hash();
          this->_hash_set = true;
        }
        return _hash;
      }

      /** Default simplify function.
          @return a simplified subtree of the AST.
       */
      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
        this->_simplified = true;
        return this->shared_from_this();
      }

      /** Default normalization function. Makes sure the subtree has a specific ordering. */
      virtual void normalize(bool recursive)
      {
        if (normalized()) // CHECK
          return;         // CHECK
        this->_normalized = true;
      }

      /** Simplification check.
          @return true if the node does not need to be simplified.
       */
      bool simplified() const
      {
        return _simplified;
      }

      /** Normalization check.
          @return true if the node does not need to be normalized.
       */
      bool normalized() const
      {
        return _normalized;
      }

      /** "Compiles" the AST node into a CExp.
          @param exp_store ExpressionStore the CExp has to be added to.
          @return the position of the CExp in the ExpressionStore.
       */
      virtual size_t compile(ExpressionStore<T>& exp_store) const = 0;

    protected:

      /** For "type system". */
      const std::type_info& type() const
      {
        return typeid(*this);
      }

      /** "Type system". Checks that a given argument is suitable as a parameter in a certain position. */
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

      /** Checks whether an ASTItem has been compiled and added to an
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
        std::ostringstream oss;
        this->Print(oss);
        exp_store[this_index]->exp = oss.str();

        // Return index and pointer
        return std::make_pair(this_index, compiled);
      }

      /** Keeps track whether the hash has been computed already. */
      mutable bool _hash_set;

      /** Hash code. */
      mutable size_t _hash;

      /** Keep track whether the ASTItem has been simplified and normalized already. */
      bool _simplified, _normalized;

      /** Virtual function to compute the hash. */
      virtual size_t compute_hash() const = 0;

    };

    /** Special ASTItem which does not need to be simplified or normalized. */
    template <typename T>
    class ASTStable : public ASTItem<T>
    {
    public:

      /** Virtual destructor. */
      virtual ~ASTStable() = default;

    protected:

      /** Default constructor. */
      ASTStable()
      {
        this->_simplified = true;
        this->_normalized = true;
      }
    };

    /** An ASTItem representing a variable. */
    template <typename T>
    class ASTVar : public ASTStable<T>
    {
    public:

      /** Constructor.
          @param name name of the variable (for printing purposes).
       */
      ASTVar(const std::string& name) : name(name) { }

      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << name;
      }

      /** Virtual destructor. */
      virtual ~ASTVar() = default;

      /** @copydoc ASTItem::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        return this->template get_or_create<CVar>(exp_store).first;
      }

    protected:

      /** Name of the variable (for printing purposes). */
      const std::string name;

      /** @copydoc ASTItem::compute_hash() */
      virtual size_t compute_hash() const
      {
        return std::hash<std::string>()(this->name);
      }

    };

    /** An ASTItem representing a variable array. */
    template <typename T>
    class ASTVarArray : public ASTStable<T>
    {
    public:

      /** Constructor.
          @param name the name of the variable array (for printing purposes).
          @param size the size of the variable array.
       */
      ASTVarArray(const std::string& name, size_t size) : name(name), size(size) { }

      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << this->name << "[" << this->size << "]";
      }

      /** Virtual destructor. */
      virtual ~ASTVarArray() = default;


      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        // Generate CExp (CVarArray)
        auto compiled_pair = this->template get_or_create<CVarArray>(exp_store);

        // Special handling for CVarArrays
        if (compiled_pair.second != nullptr)
        {
          // Get newly compiled expression (set start and size if it's a new CVarArray)
          auto compiled = std::dynamic_pointer_cast<CVarArray<T>>(compiled_pair.second);
          compiled->size = size;
        }
        return compiled_pair.first;
      }

    protected:

      /** Name of the variable array (for printing purposes). */
      const std::string name;

      /** Size of the variable array. */
      const size_t size;

      /** @copydoc ASTItem::compute_hash() */
      virtual size_t compute_hash() const
      {
        return std::hash<std::string>()(this->name);
      }
    };

    /** ASTItem representing constant value. */
    template <typename T>
    class ASTConst : public ASTStable<T>
    {
    public:

      /** Constructor.
          @param value value of the constant.
       */
      ASTConst(const T& value) : value(value) { }

      /** @copydoc Printable::Print(std::ostream& os) */
      virtual void Print(std::ostream& os) const
      {
        os << (T)(this->value);
      }

      /** Virtual destructor. */
      virtual ~ASTConst() = default;

      /** @copydoc ASTItem::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CConst>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CConst<T>>(compiled_pair.second);
          compiled->value = this->value;
        }
        return compiled_pair.first;
      }

      /** Value of the constant (safe to be public since it's constant). */
      const T value;

    protected:

      /** @copydoc ASTItem::compute_hash() */
      virtual size_t compute_hash() const
      {
        return std::hash<T>()(this->value);
      }

    };

    /** Generic class representing an operation. To be specialized to implement
        specific operators.
     */
    template <typename T>
    class ASTOp : public ASTItem<T>
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
      virtual void append_operand(const Exp<T>& operand)
      {
        // Check that the operand can be added
        this->check_compatibility(operand.p_ai);

        // Add operand
        this->operands.push_back(operand.p_ai);
      }

      /** Virtual destructor. */
      virtual ~ASTOp() = default;

      /** Normalize.
          @param recursive whether to forward the normalization to operands.
       */
      virtual void normalize(bool recursive)
      {
        if (this->normalized())
          return;
        if (recursive)
          for (auto& op : this->operands)
            op->normalize(recursive);
      }

      /** Access to operands.
          @return list of operands.
       */
      const std::list<std::shared_ptr<ASTItem<T>>>& ops() const
      {
        return operands;
      }

      /** Gets constant from operands.
          @param def default value (to be returned if first operand is not const).
          @return the value of the first operand (if const) or the default value.
       */
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

      /** Same as append_operand(const Exp<T>&), but works on ASTItems directly (for internal purpose). */
      virtual void add_operand(const std::shared_ptr<ASTItem<T>>& operand)
      {
        // Check that the operand can be added
        this->check_compatibility(operand);

        // Add operand
        this->operands.push_back(operand);
      }

      virtual void add_constant(const std::shared_ptr<ASTConst<T>>& o)
      {
        // Add const operand (at the front of the vector)
        this->operands.push_front(o);
      }

      virtual void merge_operands(const std::shared_ptr<ASTItem<T>>& other)
      {
        // Check if other ASTItem is operation
        ASTOp<T>* p_other = dynamic_cast<ASTOp<T>*>(other.get());
        if (p_other == nullptr)
          return;

        // Move operands to other's operands (generally faster because of how splice is implemented)
        p_other->operands.splice(p_other->operands.end(), this->operands);

        // Swap operands
        std::swap(this->operands, p_other->operands);
      }

      /** Constructor.
          @param sym symbol of the operation (for printing purposes).
       */
      ASTOp(const std::string& sym) : sym(sym) { }

      /** @copydoc ASTItem::compute_hash() */
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

    public:
      
      /** Symbol of the operation (for printing purposes). */
      std::string sym;

      /** Operands. */
      std::list<std::shared_ptr<ASTItem<T>>> operands;
    };

    /** Generic class for symmetric operations. */
    template <typename T>
    class ASTSymOp : public ASTOp<T>
    {
    public:
      
      /** Constructor is the same as ASTOp. */
      using ASTOp<T>::ASTOp;
      
      /** Normalize. Sorts the operands (for hashing purposes).
          @param recursive whether it should be called recursively.
       */
      virtual void normalize(bool recursive)
      {
        // Normalize operands
        ASTOp<T>::normalize(recursive);
        
        // Sort operands
        this->operands.sort([](const std::shared_ptr<ASTItem<T>>& o1, const std::shared_ptr<ASTItem<T>>& o2) {
          return (o1->type().hash_code() < o2->type().hash_code()) || (o1->type().hash_code() == o2->type().hash_code() && o1->hash() < o2->hash());
        });
      }
    };

    /** Sum operation. */
    template <typename T>
    class Sum : public ASTSymOp<T>
    {
    public:

      /** Constructor. 
          @param e1 first operand
          @param e2 second operand
       */
      Sum(const Exp<T>& e1, const Exp<T>& e2) : ASTSymOp<T>("+")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }

      /** @copydoc ASTItem::simplify() */
      virtual std::shared_ptr<ASTItem<T>> simplify()
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
            sum_of_const += std::dynamic_pointer_cast<ASTOp<T>>(*it)->steal_const(0);
            
            // Steal operands
            this->merge_operands(*it);
            
            // Erase sum
            it = this->operands.erase(it);
          }
          
          // If a constant is detected, add it to the sum_of_const
          else if ((*it)->type() == typeid(ASTConst<T>))
          {
            sum_of_const += std::dynamic_pointer_cast<ASTConst<T>>(*it)->value;
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
          this->add_constant(std::make_shared<ASTConst<T>>(sum_of_const));

        // If the sum has no elements (because of constant elimination), return zero
        if (this->operands.size() == 0)
          return std::make_shared<ASTConst<T>>(0);

        // If the sum has only one element, return it
        if (this->operands.size() == 1)
          return this->operands.front();

        // Finalize
        this->_simplified = true;
        this->normalize(false);
        return this->shared_from_this();
      }

      /** @copydoc ASTItem::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CSum>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CSum<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      /** Virtual destructor. */
      virtual ~Sum() = default;
    };

    /** Product operation. */
    template <typename T>
    class Mul : public ASTSymOp<T>
    {
    public:

      /** Constructor.
          @param e1 first operand
          @param e2 second operand
      */
      Mul(const Exp<T>& e1, const Exp<T>& e2) : ASTSymOp<T>("*")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }

      /** @copydoc ASTItem::simplify() */
      virtual std::shared_ptr<ASTItem<T>> simplify()
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
            prod_of_const *= std::dynamic_pointer_cast<ASTOp<T>>(*it)->steal_const(1);
            this->merge_operands(*it);
            it = this->operands.erase(it);
          }
          
          // Handle consts
          else if ((*it)->type() == typeid(ASTConst<T>))
          {
            prod_of_const *= std::dynamic_pointer_cast<ASTConst<T>>(*it)->value;
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
          return std::make_shared<ASTConst<T>>(prod_of_const);

        // Add new constant
        if (prod_of_const != 1)
          this->add_constant(std::make_shared<ASTConst<T>>(prod_of_const));

        // One-element product
        if (this->operands.size() == 1)
          return this->operands.front();

        // Finalize
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }

      /** @copydoc ASTItem::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMul>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CMul<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }
      
      /** Virtual destructor. */
      virtual ~Mul() = default;
    };

    /** Division operator. */
    template <typename T>
    class Div : public ASTOp<T> // not symmetric (cannot normalize by sorting the operands)
    {
    public:

      /** Constructor.
          @param e1 first operand
          @param e2 second operand
       */
      Div(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("/")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }

      /** @copydoc ASTItem::simplify() */
      virtual std::shared_ptr<ASTItem<T>> simplify()
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
          if ((*it)->type() == typeid(ASTConst<T>))
            values.push_back(std::dynamic_pointer_cast<ASTConst<T>>(*it)->value);
          (*it)->normalize(true);
        }

        // If both operands are constants, replace with result
        if (values.size() == 2)
          return std::make_shared<ASTConst<T>>(values[0] / values[1]);

        // Finalize
        this->_simplified = true;
        this->_normalized = true;

        return this->shared_from_this();
      }

      /** @copydoc ASTItem::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CDiv>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CDiv<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      /** Virtual destructor. */
      virtual ~Div() = default;
    };

    /** Modulo operation. */
    template <typename T>
    class Mod : public ASTOp<T>
    {
    public:

      /** Constructor.
          @param e1 first operand
          @param e2 second operand
       */
      Mod(const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("%")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }

      /** @copydoc ASTItem::simplify() */
      virtual std::shared_ptr<ASTItem<T>> simplify()
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
          if ((*it)->type() == typeid(ASTConst<T>))
            values.push_back(std::dynamic_pointer_cast<ASTConst<T>>(*it)->value);
          (*it)->normalize(true);
        }

        // If both operands are constants, replace operation with result
        if (values.size() == 2)
          return std::make_shared<ASTConst<T>>(values[0] % values[1]);

        // Finalize
        this->_simplified = true;
        this->_normalized = true;

        return this->shared_from_this();
      }

      /** @copydoc ASTItem::compile(ExpressionStore<T>&) */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMod>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CMod<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      /** Virtual destructor. */
      virtual ~Mod() = default;
    };

    /** Minimum between elements. */
    template <typename T>
    class Min : public ASTSymOp<T>
    {
    public:

      /** Constructor.
          @param e1 first operand
          @param e2 second operand
       */
      Min(const Exp<T>& e1, const Exp<T>& e2) : ASTSymOp<T>("+")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }

      /** @copydoc ASTItem::simplify() */
      virtual std::shared_ptr<ASTItem<T>> simplify()
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
            T stolen_const = std::dynamic_pointer_cast<ASTOp<T>>(*it)->steal_const(std::numeric_limits<T>::max());

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
          else if ((*it)->type() == typeid(ASTConst<T>))
          {
            if (!min_of_const_set)
            {
              min_of_const = std::dynamic_pointer_cast<ASTConst<T>>(*it)->value;
              min_of_const_set = true;
            }
            else
              min_of_const = std::min(min_of_const, std::dynamic_pointer_cast<ASTConst<T>>(*it)->value);
            
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
          this->add_constant(std::make_shared<ASTConst<T>>(min_of_const));

        // If we have only one operand, it must be the minimum
        if (this->operands.size() == 1)
          return this->operands.front();

        // Finalize
        this->_simplified = true;
        this->normalize(false);

        return this->shared_from_this();
      }

      /** @copydoc ASTItem::compile() */
      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMin>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CMin<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      /** Virtual destructor. */
      virtual ~Min() = default;
    };

    /** Maximum between elements. */
    template <typename T>
    class Max : public ASTSymOp<T>
    {
    public:

      /** Constructor.
          @param e1 first operand
          @param e2 second operand
       */
      Max(const Exp<T>& e1, const Exp<T>& e2) : ASTSymOp<T>("+")
      {
        this->append_operand(e1);
        this->append_operand(e2);
      }

      /** @copydoc ASTItem::simplify() */
      virtual std::shared_ptr<ASTItem<T>> simplify()
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
            T stolen_const = std::dynamic_pointer_cast<ASTOp<T>>(*it)->steal_const(std::numeric_limits<T>::min());
            
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
          else if ((*it)->type() == typeid(ASTConst<T>))
          {
            if (!max_of_const_set)
            {
              max_of_const = std::dynamic_pointer_cast<ASTConst<T>>(*it)->value;
              max_of_const_set = true;
            }
            else
              max_of_const = std::max(max_of_const, std::dynamic_pointer_cast<ASTConst<T>>(*it)->value);
            
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
          this->add_constant(std::make_shared<ASTConst<T>>(max_of_const));
        
        // If we have only one operand, it must be the minimum
        if (this->operands.size() == 1)
          return this->operands.front();
        
        // Finalize
        this->_simplified = true;
        this->normalize(false);
        
        return this->shared_from_this();
      }

      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CMax>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CMax<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~Max() = default;
    };

    /** Relational operators **/

    template <typename T>
    class Eq : public ASTSymOp<T>
    {
    public:
      Eq(const Exp<T>& e1, const Exp<T>& e2) : ASTSymOp<T>("==")
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
        auto compiled_pair = this->template get_or_create<CEq>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CEq<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~Eq() = default;
    };

    template <typename T>
    class Ne : public ASTSymOp<T>
    {
    public:
      Ne(const Exp<T>& e1, const Exp<T>& e2) : ASTSymOp<T>("!=")
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
        auto compiled_pair = this->template get_or_create<CNe>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CNe<T>>(compiled_pair.second);
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
        auto compiled_pair = this->template get_or_create<CLe>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CLe<T>>(compiled_pair.second);
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
        auto compiled_pair = this->template get_or_create<CLt>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CLt<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~Lt() = default;
    };

    template <typename T>
    class Var;

    template <typename T>
    class AllDiff : public ASTSymOp<T>
    {
    public:
      AllDiff(const std::vector<Exp<T>>& v) : ASTSymOp<T>("alldifferent")
      {
        for (const Exp<T>& e : v)
          this->append_operand(e);
      }

      AllDiff(const std::vector<Var<T>>& v) : ASTSymOp<T>("alldifferent")
      {
        for (const Var<T>& e : v)
          this->append_operand(e);
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
        auto compiled_pair = this->template get_or_create<CAllDiff>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CAllDiff<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~AllDiff() = default;
    };

    template <typename T>
    class Abs : public ASTSymOp<T>
    {
    public:
      Abs(const Exp<T>& e) : ASTSymOp<T>("abs")
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
        this->normalize(true);

        return this->shared_from_this();
      }

      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        auto compiled_pair = this->template get_or_create<CAbs>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CAbs<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~Abs() = default;
    };

    template <typename T>
    class VarArray;

    template <typename T>
    class Element : public ASTOp<T>
    {
      bool is_array;
    public:
      Element(const Exp<T>& index, const VarArray<T>& v) : ASTOp<T>("element"), is_array(true)
      {
        this->append_operand(index);
        this->append_operand(v);
      }

      Element(const Exp<T>& index, const std::vector<T>& v) : ASTOp<T>("element"), is_array(false)
      {
        this->append_operand(index);
        for (auto val : v)
          this->append_operand(Exp<T>(val));
      }

      Element(const Exp<T>& index, const std::vector<Exp<T>>& v) : ASTOp<T>("element"), is_array(false)
      {
        this->append_operand(index);
        for (const Exp<T>& e : v)
          this->append_operand(e);
      }

      virtual std::shared_ptr<ASTItem<T>> simplify()
      {
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

          (*it)->normalize(true);
        }

        this->_simplified = true;
        this->normalize(false); // all sub elements have been already normalized, so we're saving computation

        return this->shared_from_this();
      }

      virtual size_t compile(ExpressionStore<T>& exp_store) const
      {
        if (!is_array)
        {
          auto compiled_pair = this->template get_or_create<CElement>(exp_store);
          if (compiled_pair.second != nullptr)
          {
            auto compiled = std::dynamic_pointer_cast<CElement<T>>(compiled_pair.second);
            this->compile_operands(compiled_pair.first, exp_store);
          }
          return compiled_pair.first;
        }
        else
        {
          auto compiled_pair = this->template get_or_create<CArrayElement>(exp_store);
          if (compiled_pair.second != nullptr)
          {
            auto compiled = std::dynamic_pointer_cast<CArrayElement<T>>(compiled_pair.second);
            this->compile_operands(compiled_pair.first, exp_store);
          }
          return compiled_pair.first;
        }
      }

      virtual void normalize(bool recursive)
      {
        if (this->normalized())
          return;
        if (recursive && !is_array)
        {
          for (auto& op : this->operands)
            op->normalize(recursive);

          auto& first = this->operands.front();
          this->operands.erase(this->operands.begin());
          this->operands.sort([](const std::shared_ptr<ASTItem<T>>& o1, const std::shared_ptr<ASTItem<T>>& o2) {
                                return (o1->type().hash_code() < o2->type().hash_code()) || (o1->type().hash_code() == o2->type().hash_code() && o1->hash() < o2->hash());
                              });
          this->operands.push_front(first);
        }
      }

      virtual ~Element() = default;
    };

    template <typename T>
    class IfElse : public ASTOp<T>
    {
    public:

      IfElse(const Exp<T>& cond, const Exp<T>& e1, const Exp<T>& e2) : ASTOp<T>("if-else")
      {
        this->append_operand(cond);
        this->append_operand(e1);
        this->append_operand(e2);
      }

      virtual std::shared_ptr<ASTItem<T>> simplify()
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

        if (this->operands.front()->type() == typeid(ASTConst<T>))
        {
          if (dynamic_cast<ASTConst<T>*>(this->operands.front().get())->value)
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
        auto compiled_pair = this->template get_or_create<CIfElse>(exp_store);
        if (compiled_pair.second != nullptr)
        {
          auto compiled = std::dynamic_pointer_cast<CIfElse<T>>(compiled_pair.second);
          this->compile_operands(compiled_pair.first, exp_store);
        }
        return compiled_pair.first;
      }

      virtual ~IfElse() = default;
    };
  }
}

#endif // _AST_HH
