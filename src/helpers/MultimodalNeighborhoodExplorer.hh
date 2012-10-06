#if !defined(_MULTIMODALNEIGHBORHOOD_EXPLORER_HH_)
#define _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_

#include <helpers/StateManager.hh>
#include <helpers/ProhibitionManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <stdexcept>
#include <functional>
#include <algorithm>

namespace TupleUtils {
  /** Template struct whose parameters are types from 1 to N (tail indices). */
  template <int ...>
  struct tail_index { };
  
  /** Struct to generate tail_index given a certain N. */
  template <int N, int ... S>
  struct make_tail : make_tail<N-1, N-1, S ...> { };
  
  /** Struct to generate tail_index given a certain N, base case. */
  template <int ... S>
  struct make_tail<1, S ...> {
    typedef tail_index<S ...> type;
  };
  
  /** Generates a tuple's tail. */
  template <typename H, typename ... T>
  std::tuple<T...> tuple_tail(const std::tuple<H, T...>& original)
  {
    return tuple_tail(typename make_tail<std::tuple_size<std::tuple<H,T...>>::value>::type(), original);
  }
  
  /** Generates a tuple's tail. */
  template <typename H, typename ... T>
  std::tuple<std::reference_wrapper<T>...> tuple_tail(const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T>...>& original)
  {
    return tuple_tail(typename make_tail<std::tuple_size<std::tuple<H,T...>>::value>::type(), original);
  }
  
  /** Make a new tuple by accessing indices 1 to N. */
  template <typename H, typename ... T, int ... S>
  std::tuple<T...> tuple_tail(tail_index<S...>, const std::tuple<H,T...>& original)
  {
    return std::make_tuple(std::get<S>(original) ...);
  }
  
  /** Make a new tuple by accessing indices 1 to N. */
  template <typename H, typename ... T, int ... S>
  std::tuple<std::reference_wrapper<T>...> tuple_tail(tail_index<S...>, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T>...>& original)
  {
    return std::make_tuple(std::get<S>(original) ...);
  }
}

template <class Move>
class ActiveMove : public Move
{
public:
  bool active;
};

template <typename Move>
std::istream& operator>>(std::istream& is, ActiveMove<Move>& m)
{
  is >> static_cast<Move&>(m);
  return is;
}

template <typename Move>
std::ostream& operator<<(std::ostream& os, const ActiveMove<Move>& m)
{
  if (m.active)
    os << static_cast<const Move&>(m);
  return os;
}

template <typename H, typename S, typename ...T>
void print_tuple(std::ostream& os, const std::tuple<H, S, T...>& t)
{
  os << std::get<0>(t) << " ";
  auto temp_tuple_tail = TupleUtils::tuple_tail(t);
  print_tuple<S, T...>(os, temp_tuple_tail);
}

template <typename H>
void print_tuple(std::ostream& os, const std::tuple<H>& t)
{
  os << std::get<0>(t);
}

template <typename ...T>
std::ostream& operator<<(std::ostream& os, const std::tuple<T...>& t)
{
  print_tuple<T...>(os, t);
  return os;
}

template <typename ... T>
std::istream& operator>>(std::istream& is, std::tuple<T...>& t)
{
  // FIXME: when we'll really need it we'll take time to implement it
  //read_tuple(is, t);
  return is;
}

template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class MultimodalNeighborhoodExplorer : public NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...>, CFtype>
{
protected:
  
  class Call
  {
  public:
    enum Function { make_move, is_feasible_move, initialize_inactive, do_random_move, do_first_move, try_next_move, is_active, do_delta_cost_function };
    Call(Function f) : to_call(f) { }
    
    
    template <class N, class M>
    std::function<void(const N&, State&, M&)> getVoid() const throw(std::logic_error)
    {
      std::function<void(const N&, State&, M&)> f;
      switch (to_call)
      {
        case make_move:
          f = &DoMakeMove<N, M>;
          break;
        case initialize_inactive:
          f = &InitializeInactive<N, M>;
          break;
        case do_first_move:
          f = &DoFirstMove<N,M>;
          break;
        case do_random_move:
          f = &DoRandomMove<N,M>;
          break;
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }
    
    template <class N, class M>
    std::function<CFtype(const N&, State&, M&)> getCFtype() const throw(std::logic_error)
    {
      std::function<CFtype(const N&, State&, M&)> f;
      switch (to_call)
      {
          case do_delta_cost_function:
          f = &DoDeltaCostFunction<N,M>;
          break;
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }
    
    template <class N, class M>
    std::function<bool(const N&, State&, M&)> getBool() const throw(std::logic_error)
    {
      std::function<bool(const N&, State&, M&)> f;
      switch (to_call)
      {
        case is_feasible_move:
          f = &IsFeasibleMove<N,M>;
          break;
        case try_next_move:
          f = &TryNextMove<N,M>;
          break;
        case is_active:
          f = &IsActive<N,M>;
          break;
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }
    
    Function to_call;
  };
  
public:
  /** Typedefs. */
  typedef std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...> TheseMoves;
  typedef std::tuple<std::reference_wrapper<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove>> ...> TheseRefMoves;
  typedef NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...>, CFtype> SuperNeighborhoodExplorer;
  typedef std::tuple<std::reference_wrapper<BaseNeighborhoodExplorers> ...> TheseNeighborhoodExplorers;

  /** Modality of the NeighborhoodExplorer */
  virtual unsigned int Modality() const { return sizeof...(BaseNeighborhoodExplorers); }
    
  /** Constructor, takes a variable number of base NeighborhoodExplorers.  */
  MultimodalNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : SuperNeighborhoodExplorer(in, sm, name),
  nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes) ...))
  { }
    
  /**< Instantiated base NeighborhoodExplorers. */
  TheseNeighborhoodExplorers nhes;
  
  /** ExecuteAt - run function at a certain level of the tuple */
  template <typename H, typename ... T>
  static void ExecuteAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    if (iter > 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      ExecuteAt(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c, --iter);
      temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    }
  }
  
  template <class H>
  static void ExecuteAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
  }
  
  /** ExecuteFromTo - run function from a certain level of the tuple to another one */
  template <typename H, typename ... T>
  static void ExecuteFromTo (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int iter_f, int iter_t)
  {
    if (iter_f <= 0 && iter_t >= 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    if (iter_t >= 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      ExecuteFromTo(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c, --iter_f, --iter_t);
      temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    }    
  }
  
  template <class H>
  static void ExecuteFromTo (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int iter_f, int iter_t)
  {
    if (iter_t >= 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
  }

  /** ExecuteAll - run a function at every element of the tuple */
  template <typename H, typename ... T>
  static void ExecuteAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove>...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    // Apply f on head of the tuple
    std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
    f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    ExecuteAll(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c);
    temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
  }
  
  template <class H>
  static void ExecuteAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
    f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
  }
  
  /** ComputeAt - run function at a certain level of the tuple and returns its value (CFtype only) */
  template <typename H, typename ... T>
  static CFtype ComputeAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    if (iter > 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      return ComputeAt(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c, --iter);
    }
    // just to prevent warning from smart compilers
    return static_cast<CFtype>(0);
  }
  
  template <class H>
  static CFtype ComputeAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
    // just to prevent warning from smart compilers
    return static_cast<CFtype>(0);
  }
  
  /** ComputeAll - run a function at every element of the tuple and sums up the results */
  template <typename H, typename ... T>
  static CFtype ComputeAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove>...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    CFtype value = static_cast<CFtype>(0);
    // Apply f on head of the tuple
    std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
    value = f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    value += ComputeAll(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c);
    // note that the function f is allowed to have side effects on the moves
    temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    return value;
  }
  
  template <class H>
  static CFtype ComputeAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
    return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
  }
  
  /** ComputeFromTo - run function from a certain level of the tuple to another one and sums up the results */
  template <typename H, typename ... T>
  static CFtype ComputeFromTo (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int iter_f, int iter_t)
  {
    CFtype value = static_cast<CFtype>(0);
    if (iter_f <= 0 && iter_t >= 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      value = f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    if (iter_t >= 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      value += ComputeFromTo(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c, --iter_f, --iter_t);
      // note that the function f is allowed to have side effects on the moves
      temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    }
    return value;
  }
  
  template <class H>
  static void ComputeFromTo (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int iter_f, int iter_t)
  {
    if (iter_t >= 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
    return static_cast<CFtype>(0);
  }
  
  /** Compute - computes a function on each element of a tuple and returns a vector of the corresponding CFtype values */
  template <typename H, typename ... T>
  static std::vector<CFtype> Compute (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves,  const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
    std::vector<CFtype> v;
    v.push_back(f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get()));
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    std::vector<CFtype> ch = Check(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c);
    v.insert(v.end(), ch.begin(), ch.end());
    return v;
  }
  
  template <class H>
  static std::vector<CFtype> Compute (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move,  const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
    std::vector<CFtype> v;
    v.push_back(f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get()));
    return v;
  }

  /** Check - check a predicate on each element of a tuple and returns a vector of the corresponding boolean variable */
  template <typename H, typename ... T>
  static std::vector<bool> Check (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves,  const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get()));
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    std::vector<bool> ch = Check(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c);
    v.insert(v.end(), ch.begin(), ch.end());
    return v;
  }
  
  template <class H>
  static std::vector<bool> Check (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move,  const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get()));
    return v;
  }
  
  /** CheckAll - check predicate on all tuple */
  template <typename H, typename ... T>
  static bool CheckAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    // Apply f on head of the tuple
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    if (!f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get()))
      return false;
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    return CheckAll(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c);
  }
  
  template <class H>
  static bool CheckAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
  }
  
  /** CheckAny - check predicate on all tuple */
  template <typename H, typename ... T>
  static bool CheckAny (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    // Apply f on head of the tuple
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    if (f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get()))
      return true;
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    return CheckAll(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c);
  }
  
  template <class H>
  static bool CheckAny (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
  }
  
  /** CheckAt - check predicate at a certain level of a tuple */
  template <typename H, typename ... T>
  static bool CheckAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves,  const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    if (iter > 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      return CheckAt(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c, --iter);
    }
    return false;
  }
  
  template <class H>
  static bool CheckAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move,  const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
    return false;
  }
  
  /* end of Check at */
  
  virtual void MakeMove(State& st, const TheseMoves& moves) const
  {
    Call make_move(Call::Function::make_move);
    TheseRefMoves r_moves = const_cast<TheseMoves&>(moves);
    ExecuteAll(st, r_moves, this->nhes, make_move);
  }
  
  virtual bool FeasibleMove(const State& st, const TheseMoves& moves) const
  {
    Call is_feasible_move(Call::Function::is_feasible_move);
    TheseRefMoves r_moves = const_cast<TheseMoves&>(moves);
    return CheckAll(const_cast<State&>(st), r_moves, this->nhes, is_feasible_move);
  }
  
protected:
  
  template<class N, class M>
  static bool IsActive(const N& n, State& s, M& m)
  {
    return m.active;
  }
  
  template<class N, class M>
  static void DoRandomMove(const N& n, State& s, M& m)
  {
    n.RandomMove(s, m);
    m.active = true;
  }
  
  template<class N, class M>
  static void DoFirstMove(const N& n, State& s, M& m)
  {
    n.FirstMove(s, m);
    m.active = true;
  }
  
  template<class N, class M>
  static bool TryNextMove(const N& n, State& s, M& m)
  {
    M& mm = const_cast<M&>(m);
    mm.active = n.NextMove(s, mm);
    return mm.active;
  }
  
  template<class N, class M>
  static void DoMakeMove(const N& n, State& s, M& m)
  {
    if (m.active)
      n.MakeMove(s,m);
  }
  
  template<class N, class M>
  static bool IsFeasibleMove(const N& n, State& s, M& m)
  {
    if (m.active)
      return n.FeasibleMove(s, m);
    else
      return true;
  }

  template<class N, class M>
  static void InitializeInactive(const N& n, State& s, M& m)
  {
    m.active = false;
  }
  
  template<class N, class M>
  static CFtype DoDeltaCostFunction(const N& n, State& s, M& m)
  {
    return n.DeltaCostFunction(s, m);
  }
};


template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class SetUnionNeighborhoodExplorer : public MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>
{
private:

  /** Typedefs. */
//  typedef typename MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::TheseMoves TheseMoves;
//  typedef typename MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::TheseNeighborhoodExplorers TheseNeighborhoodExplorers;
  typedef MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...> SuperNeighborhoodExplorer;
  typedef typename SuperNeighborhoodExplorer::TheseRefMoves TheseRefMoves;


public:
  
  typedef typename SuperNeighborhoodExplorer::Call Call;
  
  /** Inherit constructor from superclass. Not yet, amigo. */
  // using MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::MultimodalNeighborhoodExplorer;
  
  SetUnionNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...)
  { }
  
  virtual void RandomMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    int selected = Random::Int(0, this->Modality()-1);
    
    Call initialize_inactive(Call::Function::initialize_inactive);
    Call do_random_move(SuperNeighborhoodExplorer::Call::Function::do_random_move);
    
    TheseRefMoves r_moves = moves;
    SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);
    SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, do_random_move, selected);
  }
  
  virtual void FirstMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    Call initialize_inactive(Call::Function::initialize_inactive);
    Call do_first_move(Call::Function::do_first_move);
    
    int selected = this->Modality()-1;
    
    TheseRefMoves r_moves = moves;
    SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);
       
    while (selected > 0)
    {
      try
      {
        SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, do_first_move, selected);
        break;
      }
      catch (EmptyNeighborhood e)
      {}
      selected--;
    }
    if (selected < 0)
      throw EmptyNeighborhood();
  }
  
  virtual bool NextMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    Call try_next_move(Call::Function::try_next_move);
    int selected = this->CurrentActiveMove(st, moves);
    TheseRefMoves r_moves = moves;
    if (SuperNeighborhoodExplorer::CheckAt(const_cast<State&>(st), r_moves, this->nhes, try_next_move, selected))
      return true;
    
    do
    {
      selected--;
      if (selected < 0)
        return false;
      try
      {
        TheseRefMoves r_moves = moves;
        Call do_first_move(Call::Function::do_first_move);
        SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, do_first_move, selected);
        return true;
      }
      catch (EmptyNeighborhood e)
      { }
    } while (true);
    
    return false;
  }
  
  virtual CFtype DeltaCostFunction(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call do_delta_cost_function(Call::Function::do_delta_cost_function);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    
    return SuperNeighborhoodExplorer::ComputeAt(const_cast<State&>(st), r_moves, this->nhes, do_delta_cost_function, selected);
  }
  
protected:
  int CurrentActiveMove(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    Call is_active(Call::Function::is_active);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    std::vector<bool> moves_status = SuperNeighborhoodExplorer::Check(const_cast<State&>(st), r_moves, this->nhes, is_active);
    return std::distance(moves_status.begin(), std::find_if(moves_status.begin(), moves_status.end(), [](bool element) { return element; }));
  }
};




#endif