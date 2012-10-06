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
public:
  /** Typedefs. */
  typedef std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...> TheseMoves;
  typedef std::tuple<std::reference_wrapper<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove>> ...> TheseRefMoves;
  typedef NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...>, CFtype> SuperNeighborhoodExplorer;
  typedef std::tuple<std::reference_wrapper<BaseNeighborhoodExplorers> ...> TheseNeighborhoodExplorers;

  /** Modality of the NeighborhoodExplorer */
  virtual unsigned int Modality() const { return sizeof...(BaseNeighborhoodExplorers); }
  
protected:
  
  /** Constructor, takes a variable number of base NeighborhoodExplorers.  */
  MultimodalNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : SuperNeighborhoodExplorer(in, sm, name),
  nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes) ...))
  { }
  
  /**< Instantiated base NeighborhoodExplorers. */
  TheseNeighborhoodExplorers nhes;
  
  /** TupleDispatching helper classes/functions */
  
  
  class Call
  {
  public:
    enum Function { initialize_inactive, initialize_active, make_move, is_feasible_move, do_random_move, do_first_move, try_next_move, is_active, do_delta_cost_function };
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
        case initialize_active:
          f = &InitializeActive<N, M>;
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
  
  template <class TupleOfMoves, class TupleOfNHEs, std::size_t N>
  struct TupleDispatcher
  {
    static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int iter)
    {
      if (iter == 0)
      {
        auto& this_nhe = std::get<N>(temp_nhes).get();
        auto& this_move = std::get<N>(temp_moves).get();
        std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
        f(this_nhe, st, this_move);
      }
      else if (iter > 0)
        TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ExecuteAt(st, temp_moves, temp_nhes, c, --iter);
#if defined(DEBUG)
      else
        throw std::logic_error("In function ExecuteAt (recursive case) iter is less than zero");
#endif
    }
    
    static void ExecuteAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
      TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ExecuteAll(st, temp_moves, temp_nhes, c);
      f(this_nhe, st, this_move);
    }
    
    static CFtype ComputeAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int iter)
    {
      if (iter == 0)
      {
        auto& this_nhe = std::get<N>(temp_nhes).get();
        auto& this_move = std::get<N>(temp_moves).get();
        std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
        return f(this_nhe, st, this_move);
      }
      else if (iter > 0)
        return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ComputeAt(st, temp_moves, temp_nhes, c, --iter);
#if defined(DEBUG)
      else
        throw std::logic_error("In function ComputeAt (recursive case) iter is less than zero");
#endif
      return static_cast<CFtype>(0);
    }
    
    static CFtype ComputeAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
      return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ComputeAll(st, temp_moves, temp_nhes, c) + f(this_nhe, st, this_move);
    }
    
    static std::vector<bool> Check(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      std::vector<bool> current, others;
      current.push_back(f(this_nhe, st, this_move));
      others = TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::Check(st, temp_moves, temp_nhes, c);
      current.insert(current.begin(), others.begin(), others.end());
      return current;
    }
    
    static bool CheckAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      
      if (!f(this_nhe, st, this_move))
        return false;
      
      return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAll(st, temp_moves, temp_nhes, c);
    }
    
    static bool CheckAny(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      
      if (f(this_nhe, st, this_move))
        return true;
      
      return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAll(st, temp_moves, temp_nhes, c);
    }
    
    static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int iter)
    {
      if (iter == 0)
      {
        auto& this_nhe = std::get<N>(temp_nhes).get();
        auto& this_move = std::get<N>(temp_moves).get();
        std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
        return f(this_nhe, st, this_move);
      }
      else if (iter > 0)
        return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAt(st, temp_moves, temp_nhes, c, --iter);
#if defined(DEBUG)
      else
        throw std::logic_error("In function CheckAt (recursive case) iter is less than zero");
#endif
      return false;
    }
  };
  
  template <class TupleOfMoves, class TupleOfNHEs>
  struct TupleDispatcher<TupleOfMoves, TupleOfNHEs, 0>
  {
    static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int iter)
    {
      if (iter == 0)
      {
        auto& this_nhe = std::get<0>(temp_nhes).get();
        auto& this_move = std::get<0>(temp_moves).get();
        std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
        f(this_nhe, st, this_move);
      }
#if defined(DEBUG)
      else
        throw std::logic_error("In function ExecuteAt (base case) iter is not zero");
#endif
    }
    
    static void ExecuteAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
      f(this_nhe, st, this_move);
    }
    
    static CFtype ComputeAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int iter)
    {
      if (iter == 0)
      {
        auto& this_nhe = std::get<0>(temp_nhes).get();
        auto& this_move = std::get<0>(temp_moves).get();
        std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
        return f(this_nhe, st, this_move);
      }
#if defined(DEBUG)
      else
        throw std::logic_error("In function ComputeAt (base case) iter is not zero");
#endif
      return static_cast<CFtype>(0);
    }
    
    static CFtype ComputeAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
      return f(this_nhe, st, this_move);
    }
    
    static std::vector<bool> Check(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      std::vector<bool> current;
      current.push_back(f(this_nhe, st, this_move));
      return current;
    }
    
    static bool CheckAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      
      return f(this_nhe, st, this_move);
    }
    
    static bool CheckAny(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      
      return f(this_nhe, st, this_move);
    }
    
    static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int iter)
    {
      if (iter == 0)
      {
        auto& this_nhe = std::get<0>(temp_nhes).get();
        auto& this_move = std::get<0>(temp_moves).get();
        std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
        return f(this_nhe, st, this_move);
      }
#if defined(DEBUG)
      else
        throw std::logic_error("In function CheckAt (base case) iter is not zero");
#endif
      return false;
    }
  };
  
  /** ExecuteAt - runs a function at a specific element of the tuple */
  template <class ...NHEs>
  static void ExecuteAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                        const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int iter)
  {
    TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ExecuteAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - iter);
  }
  
  /** ExecuteAll - runs a function at every element of the tuple */
  template <class ...NHEs>
  static void ExecuteAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                         const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ExecuteAll(st, moves, nhes, c);
  }
  
  /** ComputeAt - runs a function at a certain level of the tuple and returns its value (CFtype only) */
  template <class ...NHEs>
  static CFtype ComputeAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                          const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int iter)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ComputeAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - iter);
  }
  
  /** ComputeAll - runs a function at every element of the tuple and sums up the results */
  template <class ...NHEs>
  static CFtype ComputeAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                           const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ComputeAll(st, moves, nhes, c);
  }
  
  /** Check - checks a predicate on each element of a tuple and returns a vector of the corresponding boolean variable */
  template <class ...NHEs>
  static std::vector<bool> Check(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                                 const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::Check(st, moves, nhes, c);
  }
  
  /** CheckAll - checks a predicate on all tuple */
  template <class ...NHEs>
  static bool CheckAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                       const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAll(st, moves, nhes, c);
  }
  
  /** CheckAny - checks a predicate on all tuple */
  template <class ...NHEs>
  static bool CheckAny(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                       const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAny(st, moves, nhes, c);
  }
  
  /** CheckAt - checks a predicate at a certain level of the tuple and returns its value */
  template <class ...NHEs>
  static CFtype CheckAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                        const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int iter)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - iter);
  }
  
  // helper functions
  
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
  static void InitializeActive(const N& n, State& s, M& m)
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
  typedef typename SuperNeighborhoodExplorer::Call Call;

public:
  
  
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
    
    int selected = 0;
    
    TheseRefMoves r_moves = moves;
    SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);
       
    while (selected < this->Modality())
    {
      try
      {
        SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, do_first_move, selected);
        break;
      }
      catch (EmptyNeighborhood e)
      {}
      selected++;
    }
    if (selected == this->Modality())
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
      selected++;
      if (selected == this->Modality())
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
  
  virtual void MakeMove(State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call make_move(Call::Function::make_move);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    SuperNeighborhoodExplorer::ExecuteAt(st, r_moves, this->nhes, make_move, selected);
  }
  
  virtual bool FeasibleMove(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call is_feasible_move(Call::Function::is_feasible_move);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    return SuperNeighborhoodExplorer::CheckAt(const_cast<State&>(st), r_moves, this->nhes, is_feasible_move, selected);
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

template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class CartesianProductNeighborhoodExplorer : public MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>
{
private:
  
  /** Typedefs. */
  //  typedef typename MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::TheseMoves TheseMoves;
  //  typedef typename MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::TheseNeighborhoodExplorers TheseNeighborhoodExplorers;
  typedef MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...> SuperNeighborhoodExplorer;
  typedef typename SuperNeighborhoodExplorer::TheseRefMoves TheseRefMoves;
  typedef typename SuperNeighborhoodExplorer::Call Call;

#if defined(DEBUG)
  void VerifyAllActives(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const throw (std::logic_error)
  {
    Call is_active(SuperNeighborhoodExplorer::Call::Function::is_active);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    std::vector<bool> active = SuperNeighborhoodExplorer::Check(const_cast<State&>(st), r_moves, this->nhes, is_active);
    for (bool v : active)
    {
      if (!v)
        throw std::logic_error("Some of the moves were not active in a composite CartesianProduct neighborhood explorer");
    }
  }
#endif
  
public:
  
  
  /** Inherit constructor from superclass. Not yet, amigo. */
  // using MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::MultimodalNeighborhoodExplorer;
  
  CartesianProductNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...)
  { }
  
  virtual void RandomMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    Call do_random_move(SuperNeighborhoodExplorer::Call::Function::do_random_move);
    Call make_move(SuperNeighborhoodExplorer::Call::Function::make_move);
    
    TheseRefMoves r_moves = moves;
    std::vector<State> temp_states(this->Modality() - 1); // the chain of states (including the first one, but excluding the last)
    
    temp_states[0] = st;
    SuperNeighborhoodExplorer::ExecuteAll(temp_states[0], r_moves, this->nhes, do_random_move);
    SuperNeighborhoodExplorer::ExecuteAt(temp_states[0], r_moves, this->nhes, make_move, 0);
    for (int i = 1; i < this->Modality() - 1; i++)
    {
      temp_states[i] = temp_states[i - 1];
      SuperNeighborhoodExplorer::ExecuteAll(temp_states[i], r_moves, this->nhes, do_random_move);
      SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, 0);
    }
    SuperNeighborhoodExplorer::ExecuteAt(temp_states[this->Modality() - 2], r_moves, this->nhes, do_random_move, this->Modality() - 1);
    
    // just for debugging
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
  }
  
  virtual void FirstMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    Call do_first_move(Call::Function::do_first_move);
    Call make_move(Call::Function::make_move);
    Call try_next_move(Call::Function::try_next_move);
    
    int i = 0;
    TheseRefMoves r_moves = moves;

    std::vector<State> temp_states(this->Modality(), State(this->in)); // the chain of states
    temp_states[0] = st; // the first state is a copy of to st
    
    // this call order is a small optimization, since first move could fail already on the first state we save a state copy
    SuperNeighborhoodExplorer::ExecuteAt(temp_states[0], r_moves, this->nhes, do_first_move, 0);
    if (this->Modality() == 1)
      return; // nothing else to do (it should never be the case, it's here just for compatibility)    
    temp_states[1] = temp_states[0];
    SuperNeighborhoodExplorer::ExecuteAt(temp_states[1], r_moves, this->nhes, make_move, 0);
    
    i = 1;
    while (true)
    {
      try
      {
        // this call order is a small optimization, since first move could fail on the previous state we save a state copy
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, do_first_move, i);
        if (i == this->Modality() - 1) {
          // the whole chain of moves has been dispatched
#if defined(DEBUG)
          VerifyAllActives(st, moves);
#endif
          return;
        }
        temp_states[i + 1] = temp_states[i];
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[i + 1], r_moves, this->nhes, make_move, i);
      }
      catch (EmptyNeighborhood e)
      {
        while (i > 0)
        {
          i--; // backtrack
          if (SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, try_next_move, i))
          {
            temp_states[i + 1] = temp_states[i];
            SuperNeighborhoodExplorer::ExecuteAt(temp_states[i + 1], r_moves, this->nhes, make_move, i);
            break;
          }     
        }
        if (i < 0)
        {
          // no combination whatsoever could be extended as a "first move"
          throw EmptyNeighborhood();
        }
      }
      i++;
    }
  }
  
  virtual bool NextMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    Call do_first_move(Call::Function::do_first_move);
    Call make_move(Call::Function::make_move);
    Call try_next_move(Call::Function::try_next_move);
    
    int i = 0;
    TheseRefMoves r_moves = moves;
    
    std::vector<State> temp_states(this->Modality(), State(this->in)); // the chain of states
    temp_states[0] = st; // the first state is a copy of to st
        
    // create and initialize all the remaining states in the chain
    for (int i = 1; i < this->Modality(); i++)
    {
      temp_states[i] = temp_states[i - 1];
      SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, i - 1);
    }
    
    if (SuperNeighborhoodExplorer::CheckAt(temp_states[this->Modality() - 1], r_moves, this->nhes, try_next_move, this->Modality() - 1))
      return true;
    
    i = this->Modality() - 1;
    bool backtracking = true;
    while (i < this->Modality())
    {
      // backtracking to the first available component that has a next move
      while (backtracking && i > 0)
      {
        i--; // backtrack
        if (SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, try_next_move, i))
        {
          // a partial next move (up to i) has been found
          temp_states[i + 1] = temp_states[i];
          SuperNeighborhoodExplorer::ExecuteAt(temp_states[i + 1], r_moves, this->nhes, make_move, i);
          i++;
          break;
        }
      }
      if (i == 0)
        return false;
      backtracking = false;
      // forward trying a set of first moves
      try
      {
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, do_first_move, i);
        if (i == this->Modality() - 1)
          return true;
        temp_states[i + 1] = temp_states[i];
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[i + 1], r_moves, this->nhes, make_move, i);
        i++;
      }
      catch (EmptyNeighborhood e)
      {
        backtracking = true;
      }
    }
    return true;
  }
  
  virtual CFtype DeltaCostFunction(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
    // FIXME: at present is buggy
    Call do_delta_cost_function(Call::Function::do_delta_cost_function);
    Call make_move(Call::Function::make_move);

    CFtype sum = static_cast<CFtype>(0);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    
    std::vector<State> temp_states(this->Modality(), State(this->in)); // the chain of states
    temp_states[0] = st; // the first state is a copy of to st    
    // create and initialize all the remaining states in the chain
    sum = SuperNeighborhoodExplorer::ComputeAt(temp_states[0], r_moves, this->nhes, do_delta_cost_function, 0);
    for (int i = 1; i < this->Modality(); i++)
    {
      temp_states[i] = temp_states[i - 1];
      SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, i - 1);
      sum += SuperNeighborhoodExplorer::ComputeAt(temp_states[i], r_moves, this->nhes, do_delta_cost_function, i);
    }
    
    return sum;
  }
  
  virtual void MakeMove(State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
    Call make_move(Call::Function::make_move);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    SuperNeighborhoodExplorer::ExecuteAll(st, r_moves, this->nhes, make_move);
  }
  
  virtual bool FeasibleMove(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
    Call is_feasible_move(Call::Function::is_feasible_move);
    Call make_move(Call::Function::make_move);
    TheseRefMoves r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);

    std::vector<State> temp_states(this->Modality(), State(this->in)); // the chain of states
    temp_states[0] = st; // the first state is a copy of to st
                         // create and initialize all the remaining states in the chain
    if (!SuperNeighborhoodExplorer::CheckAt(temp_states[0], r_moves, this->nhes, is_feasible_move, 0))
      return false;
    for (int i = 1; i < this->Modality(); i++)
    {
      temp_states[i] = temp_states[i - 1];
      SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, i - 1);
      if (!SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, is_feasible_move, i))
        return false;
    }
    return true;
  }
  
protected:
  // TODO: a state information needed to speed up computation and not
  // to recompute the effect of the whole chain of moves
  // TODO: find a mechanism to be sure that state is consistent across calls
  //std::vector<State> temp_states;
};



#endif