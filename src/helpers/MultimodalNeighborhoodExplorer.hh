#if !defined(_MULTIMODALNEIGHBORHOOD_EXPLORER_HH_)
#define _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_

#include <helpers/DeltaCostComponent.hh>
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
  std::tuple<T...> tuple_tail(const std::tuple<H, T...>& original) {
    return tuple_tail(typename make_tail<std::tuple_size<std::tuple<H,T...>>::value>::type(), original);
  }
  
  /** Make a new tuple by accessing indices 1 to N. */
  template <typename H, typename ... T, int ... S>
  std::tuple<T...> tuple_tail(tail_index<S...>, const std::tuple<H,T...>& original) {
    return std::make_tuple(std::get<S>(original) ...);
  }
}

template <class ... Move>
class ActiveMove : public Move ... {
public:
  bool active;
};

template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class MultimodalNeighborhoodExplorer : public NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...>, CFtype>
{
protected:
  
  class Call
  {
  public:
    enum Function { make_move, is_feasible_move, initialize_inactive, do_random_move, do_first_move, try_next_move, is_active, do_delta_cost_function, do_delta_shifted_cost_function };
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
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }
    
    template <class N, class M>
    std::function<bool(const N&, const State&, const M&)> getBool() const throw(std::logic_error)
    {
      std::function<bool(const N&, const State&, const M&)> f;
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
    
    template <class N, class M>
    std::function<CFtype(const N&, const State&, const M&)> getCFtype() const throw(std::logic_error)
    {
      std::function<CFtype(const N&, const State&, const M&)> f;
      switch (to_call)
      {
        case do_delta_cost_function:
          f = &DoDeltaCostFunction<N,M>;
          break;
          
        case do_delta_shifted_cost_function:
          f = &DoDeltaShiftedCostFunction<N,M>;
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
  typedef NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...>, CFtype> SuperNeighborhoodExplorer;
  typedef std::tuple<BaseNeighborhoodExplorers ...> TheseNeighborhoodExplorers;

  /** Modality of the NeighborhoodExplorer */
  virtual unsigned int Modality() const { return sizeof...(BaseNeighborhoodExplorers); }
    
  /** Constructor, takes a variable number of base NeighborhoodExplorers.  */
  MultimodalNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : SuperNeighborhoodExplorer(in, sm, name),
    nhes(std::make_tuple(nhes ...))
  { }
    
  /**< Instantiated base NeighborhoodExplorers. */
  TheseNeighborhoodExplorers nhes;
  
  /** UnpackAt - run function at a certain level of the tuple */
  template <typename H, typename ... T>
  static void UnpackAt (State& st, std::tuple<ActiveMove<typename H::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves, const std::tuple<H, T ...> temp_nhes, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves));
    }
    if (iter > 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      UnpackAt(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c, --iter);
      temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    }
  }
  
  template <class H>
  static void UnpackAt (State& st, std::tuple<ActiveMove<typename H::ThisMove>>& temp_move, const std::tuple<H> temp_nhe, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
    }
  }

  /** UnpackAll - run a function at every element of the tuple */
  template <typename H, typename ... T>
  static void UnpackAll (State& st, std::tuple<ActiveMove<typename H::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  std::tuple<H, T ...> temp_nhes, const Call& c)
  {
    // Apply f on head of the tuple
    std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
    f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves));
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    UnpackAll(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), c);
    temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
  }
  
  template <class H>
  static void UnpackAll (State& st, std::tuple<ActiveMove<typename H::ThisMove>>& temp_move,  std::tuple<H> temp_nhe, const Call& c)
  {
    std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
    f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }

  /** Check - check a predicate on each element of a tuple and returns a vector of the corresponding boolean variable */
  template <typename H, typename ... T>
  static std::vector<bool> Check (const State& st, const std::tuple<ActiveMove<typename H::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<H, T ...> temp_nhes, const Call& c)
  {
    std::function<bool(const H&, const State&, const ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)));
    std::vector<bool> ch = Check(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes), c);
    v.insert(v.end(), ch.begin(), ch.end());
    return v;
  }
  
  template <class H>
  static std::vector<bool> Check (const State& st, const std::tuple<ActiveMove<typename H::ThisMove>>& temp_move,  const std::tuple<H> temp_nhe, const Call& c)
  {
    std::function<bool(const H&, const State&, const ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhe), st, std::get<0>(temp_move)));
    return v;
  }
  
  /** CheckAll - check predicate on all tuple */
  template <typename H, typename ... T>
  static bool CheckAll (const State& st, const std::tuple<ActiveMove<typename H::ThisMove>, const ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<H, T ...> temp_nhes, const Call& c)
  {
    std::function<bool(const H&, const State&, const ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)) && CheckAll(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes), c);
  }
  
  template <class H>
  static bool CheckAll(const State& st, const std::tuple<ActiveMove<typename H::ThisMove>>& temp_move, const std::tuple<H> temp_nhe, const Call& c)
  {
    std::function<bool(const H&, const State&, const ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    return f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }
  
  /** CheckAny - check predicate on all tuple */
  template <typename F, typename S, typename ... T>
  static bool CheckAny (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)) || CheckAny(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes));
  }
  
  template <class F>
  static bool CheckAny(const State& st, const std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  const std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    return f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }
  
  /** CheckAt - check predicate at a certain level of a tuple */
  template <typename H, typename ... T>
  static bool CheckAt (const State& st, const std::tuple<ActiveMove<typename H::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<H, T ...> temp_nhes, const Call& c, int iter)
  {
    if (iter == 0)
    {
      std::function<bool(const H&, const State&, const ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves));
    }
    if (iter > 0)
      return CheckAt(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes), c, --iter);
    return false;
  }
  
  template <class H>
  static bool CheckAt (const State& st, const std::tuple<ActiveMove<typename H::ThisMove>>& temp_move,  const std::tuple<H> temp_nhe, const Call& c, int iter)
  {
    std::function<bool(const H&, const State&, const ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    return (iter == 0 && f(std::get<0>(temp_nhe), st, std::get<0>(temp_move)));
  }
  
  virtual void MakeMove(State& st, const TheseMoves& moves) const
  {
    Call c(Call::Function::make_move);
    UnpackAll(st, const_cast<TheseMoves&>(moves), this->nhes, c);
  }
  
  virtual bool FeasibleMove(const State& st, const TheseMoves& moves) const
  {
    Call c(Call::Function::is_feasible_move);
    return CheckAll(st, moves, this->nhes, c);
  }
  
protected:
  
  template<class N, class M>
  static bool IsActive(const N& n, const State& s, const M& m)
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
  static bool TryNextMove(const N& n, const State& s, const M& m)
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
  static bool IsFeasibleMove(const N& n, const State& s, const M& m)
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
  static CFtype DoDeltaCostFunction(const N& n, const State& s, const M& m)
  {
    return n.DeltaCostFunction(s, m);
  }
  
  template<class N, class M>
  static CFtype DoDeltaShiftedCostFunction(const N& n, const State& s, const M& m)
  {
    return n.DeltaShiftedCostFunction(s, m);
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
    
    SuperNeighborhoodExplorer::UnpackAll(const_cast<State&>(st), moves, this->nhes, initialize_inactive);
    SuperNeighborhoodExplorer::UnpackAt(const_cast<State&>(st), moves, this->nhes, do_random_move, selected);
  }
  
  virtual void FirstMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    Call initialize_inactive(Call::Function::initialize_inactive);
    Call do_first_move(Call::Function::do_first_move);
    
    int selected = this->Modality()-1;
    
    SuperNeighborhoodExplorer::UnpackAll(const_cast<State&>(st), moves, this->nhes, initialize_inactive);
       
    while (selected > 0)
    {
      try
      {
        SuperNeighborhoodExplorer::UnpackAt(const_cast<State&>(st), moves, this->nhes, do_first_move, selected);
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
    if (SuperNeighborhoodExplorer::CheckAt(st, moves, this->nhes, try_next_move, selected))
      return true;
    
    do
    {
      selected--;
      try
      {
        Call do_first_move(Call::Function::do_first_move);
        SuperNeighborhoodExplorer::UnpackAt(const_cast<State&>(st), moves, this->nhes, do_first_move, selected);
        return true;
      }
      catch (EmptyNeighborhood e)
      { }
    } while (selected > 0);
    
    return false;
  }
  
  virtual CFtype DeltaCostFunction(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call do_delta_cost_function(Call::Function::do_delta_cost_function);

    return SuperNeighborhoodExplorer::UnpackAt(st, moves, this->nhes, do_delta_cost_function, selected);
  }

  virtual CFtype DeltaShiftedCostFunction(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call do_delta_shifted_cost_function(Call::Function::do_delta_shifted_cost_function);

    return SuperNeighborhoodExplorer::UnpackAt(st, moves, this->nhes, do_delta_shifted_cost_function, selected);
  }
  
protected:
  int CurrentActiveMove(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    Call is_active(Call::Function::is_active);
    std::vector<bool> moves_status = SuperNeighborhoodExplorer::Check(st, moves, this->nhes, is_active);
    return std::distance(moves_status.begin(), std::find_if(moves_status.begin(), moves_status.end(), [](bool& element) { return element; }));
  }
};




#endif