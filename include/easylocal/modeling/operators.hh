#ifndef _OPERATORS_HH
#define _OPERATORS_HH


#include <limits>
#include "easylocal/modeling/expression.hh"

#include <iostream>

using std::cout;
using std::endl;

namespace EasyLocal {
  
  /** Operators. These are used to build complex expressions on top of basic
   *  components such as variables, constants, and arrays. Note: arrays are
   *  in fact already built on top of expressions, but they act as a single
   *  expression enabling, for instance, to post expressions such as 
   *  Different and Element.
   */
  
  namespace Modeling {
    
    /** Construction operators. Used to build variables, constants, and so on
     *  and so forth.
     */
    
    template <typename T>
    static std::shared_ptr<Exp<T>> variable(const std::string& name, T lb = std::numeric_limits<T>::min(), T ub = std::numeric_limits<T>::max())
    {
      // FIXME: currently bounds are ignored
      return std::make_shared<Var<T>>(name, lb, ub);
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> constant(const T& val)
    {
      // FIXME: currently bounds are ignored
      return std::make_shared<Const<T>>(val);
    }
    
    /** Arithmetic operators. These are implemented by defining the modification
     *  operators (+=, -=, ...), and then defining the expression-creation 
     *  operators on top of them.
     */
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator+=(std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      e1 = std::make_shared<Sum<T>>(e1, e2);
      e1 = e1->simplify();
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator+=(std::shared_ptr<Exp<T>>& e1, const T& v)
    {
      if (v != 0)
      {
        e1 = std::make_shared<Sum<T>>(std::make_shared<Const<T>>(v), e1);
        e1 = e1->simplify();
      }
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator+(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = e1;
      r += e2; // forward to operator+=(std::shared_ptr<Exp<T>>&, const std::shared_ptr<Exp<T>>&)
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator+(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      auto r = e;
      r += v; // forward to operator+=(std::shared_ptr<Exp<T>>&, const T&)
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator+(const T& v, const std::shared_ptr<Exp<T>> e)
    {
      return e + v; // forward to operator+(const std::shared_ptr<Exp<T>>&, const T&) (commutative)
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator-=(std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto _e2 = std::make_shared<Mul<T>>(std::make_shared<Const<T>>(-1), e2);
      e1 = std::make_shared<Sum<T>>(e1, _e2);
      e1 = e1->simplify();
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator-=(std::shared_ptr<Exp<T>>& e1, const T& v)
    {
      if (v != 0)
      {
        e1 = std::make_shared<Sum<T>>(e1, std::make_shared<Const<T>>(-v));
        e1 = e1->simplify();
      }
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator-(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      
      auto r = e1;
      r -= e2; // forward to operator-=(std::shared_ptr<Exp<T>>&, const std::shared_ptr<Exp<T>>&)
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator-(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      if (v != 0)
      {
        auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v));
        r -= e; // forward to operator-=(std::shared_ptr<Exp<T>>&, const std::shared_ptr<Exp<T>>&)
        return r;
      }
      else
      {
        auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Mul<T>>(std::make_shared<Const<T>>(-1), e)); // -v == mul(-1, v) without forwarding
        r = r->simplify();
        return r;
      }
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator-(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      auto r = e;
      r -= v; // forward to operator-=(std::shared_ptr<Exp<T>>&, T)
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator-(const std::shared_ptr<Exp<T>>& e)
    {
      auto _e = std::static_pointer_cast<Exp<T>>(std::make_shared<Mul<T>>(std::make_shared<Const<T>>(-1), e));
      _e = _e->simplify();
      return _e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator-(std::shared_ptr<Exp<T>>& e)
    {
      e = std::make_shared<Mul<T>>(std::make_shared<Const<T>>(-1), e);
      e = e->simplify();
      return e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator*=(std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      e1 = std::make_shared<Mul<T>>(e1, e2);
      e1 = e1->simplify();
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator*=(std::shared_ptr<Exp<T>>& e1, const T& v)
    {
      if (v == 0)
        e1 = std::make_shared<Const<T>>(0);
      if (v != 1)
      {
        e1 = std::make_shared<Mul<T>>(std::make_shared<Const<T>>(v), e1);
        e1 = e1->simplify();
      }
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator*(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = e1;
      r *= e2; // forward to operator*=(std::shared_ptr<Exp<T>>&, const std::shared_ptr<Exp<T>>&)
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator*(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      auto r = e;
      r *= v; // forward to operator*=(std::shared_ptr<Exp<T>>&, const T&)
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator*(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return e * v;  // operator*(const std::make_shared<Exp<T>>&, const T&)
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator/=(std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      e1 = std::make_shared<Div<T>>(e1, e2);
      e1 = e1->simplify();
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator/=(std::shared_ptr<Exp<T>>& e1, const T& v)
    {
      if (v != 0)
      {
        if (v != 1)
        {
          e1 = std::make_shared<Div<T>>(e1, std::make_shared<Const<T>>(v));
          e1 = e1->simplify();
        }
        return e1;
      }
      throw std::logic_error("Trying to compute division by zero");
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator/(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = e1;
      r /= e2;  // forward to operator/=(std::make_shared<Exp<T>>&, const std::make_shared<Exp<T>>&)
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator/(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      if (v != 0) {
        auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v));
        r /= e; // forward to operator/=(std::make_shared<Exp<T>>&, const std::make_shared<Exp<T>>&)
        return r;
      }
      else
        return std::make_shared<Const<T>>(0);
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator/(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      auto r = e;
      r /= v;
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator%=(std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      e1 = std::make_shared<Mod<T>>(e1, e2);
      e1 = e1->simplify();
      return e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>>& operator%=(std::shared_ptr<Exp<T>>& e1, const T& v)
    {
      if (v != 0)
      {
        if (v != 1)
        {
          e1 = std::make_shared<Mod<T>>(e1, std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)));
          e1 = e1->simplify();
        }
        else
          e1 = std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(0));
        return e1;
      }
      throw std::logic_error("Trying to compute modulo operation to zero");
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator%(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = e1;
      r %= e2;
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator%(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v));
      r %= e;
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator%(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      auto r = e;
      r %= v;
      return r;
    }
    
    /** Relational operators (<, >, ==, !=, ...). Return binary answers. */
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator==(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Eq<T>>(e1, e2));
      r = r->simplify();
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator==(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) == e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator==(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) == e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator!=(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Ne<T>>(e1, e2));
      r = r->simplify();
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator!=(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) != e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator!=(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) != e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator<=(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Le<T>>(e1, e2));
      r = r->simplify();
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator<=(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return e <= std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v));
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator<=(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) <= e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator<(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Lt<T>>(e1, e2));
      r = r->simplify();
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator<(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return e < std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v));
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator<(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) < e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator>=(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      return e2 <= e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator>=(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) <= e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator>=(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return e <= std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v));
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator>(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      return e2 < e1;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator>(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)) < e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> operator>(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return e < std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v));
    }
    
    /** More complex operators (different, element, min, max, ...). */
    
    template <typename T>
    static std::shared_ptr<Exp<T>> min(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Min<T>>(e1, e2));
      r = r->simplify();
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> min(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return min(std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)), e);
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> min(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return min(e, std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)));
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> max(const std::shared_ptr<Exp<T>>& e1, const std::shared_ptr<Exp<T>>& e2)
    {
      auto r = std::static_pointer_cast<Exp<T>>(std::make_shared<Max<T>>(e1, e2));
      r = r->simplify();
      return r;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> max(const T& v, const std::shared_ptr<Exp<T>>& e)
    {
      return max(std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)), e);
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> max(const std::shared_ptr<Exp<T>>& e, const T& v)
    {
      return max(e, std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v)));
    }
    
    template <typename T>                        // we pass an Array, which is an Exp (Exp)
    static std::shared_ptr<Exp<T>> n_values(const std::shared_ptr<Exp<T>>& v)
    {
      auto e = std::static_pointer_cast<Exp<T>>(std::make_shared<NValues<T>>(v));
      e = e->simplify();
      return e;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> abs(const std::shared_ptr<Exp<T>>& e)
    {
      auto t = std::static_pointer_cast<Exp<T>>(std::make_shared<Abs<T>>(e));
      
      
      t = t->simplify();
      return t;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> element(const std::shared_ptr<Exp<T>>& index, const std::shared_ptr<Exp<T>>& v)
    {
      auto t = std::static_pointer_cast<Exp<T>>(std::make_shared<Element<T>>(index, v));
      t = t->simplify();
      return t;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> element(const std::shared_ptr<Exp<T>>& index, const std::vector<T>& v)
    {
      auto t = std::static_pointer_cast<Exp<T>>(std::make_shared<Element<T>>(index, v));
      t = t->simplify();
      return t;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> if_then_else(const std::shared_ptr<Exp<T>>& cond, const std::shared_ptr<Exp<T>>& e_then, const std::shared_ptr<Exp<T>>& e_else)
    {
      auto t = std::static_pointer_cast<Exp<T>>(std::make_shared<IfThenElse<T>>(cond, e_then, e_else));
      t = t->simplify();
      return t;
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> if_then_else(const std::shared_ptr<Exp<T>>& cond, const T& v_then, const std::shared_ptr<Exp<T>>& e_else)
    {
      return if_then_else(
        cond,
        std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v_then)),
        e_else
      );
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> if_then_else(const std::shared_ptr<Exp<T>>& cond, const std::shared_ptr<Exp<T>>& e_then, const T& v_else)
    {
      return if_then_else(
        cond,
        e_then,
        std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v_else))
      );
    }
    
    template <typename T>
    static std::shared_ptr<Exp<T>> if_then_else(const std::shared_ptr<Exp<T>>& cond, const T& v_then, const T& v_else)
    {
      return if_then_else(
        cond,
        std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v_then)),
        std::static_pointer_cast<Exp<T>>(std::make_shared<Const<T>>(v_else))
      );
    }
    
    template <typename T>
    std::ostream& operator<<(std::ostream& os, const std::shared_ptr<Exp<T>>& e)
    {
      os << *e;
      return os;
    }

    
  }
}

#endif // _OPERATORS_HH