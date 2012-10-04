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
  std::tuple<T...> tuple_tail(std::tuple<H, T...>& original) {
    return tuple_tail(typename make_tail<std::tuple_size<std::tuple<H,T...>>::value>::type(), original);
  }
  
  /** Make a new tuple by accessing indices 1 to N. */
  template <typename H, typename ... T, int ... S>
  std::tuple<T...> tuple_tail(tail_index<S...>, std::tuple<H,T...>& original) {
    return std::make_tuple(std::get<S>(original) ...);
  }
}

template <class Move>
class ActiveMove : public Move {
public:
  bool active;
};

template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class MultimodalNeighborhoodExplorer : public NeighborhoodExplorer<Input, State, CFtype, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove ...>>>
{
public:

  /** Typedefs. */
  typedef std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove ...>> TheseMoves;
  typedef NeighborhoodExplorer<Input, State, CFtype, TheseMoves> SuperNeighborhoodExplorer;
  typedef std::tuple<BaseNeighborhoodExplorers ...> TheseNeighborhoodExplorers;

  /** Modality of the NeighborhoodExplorer */
  virtual int Modality() { return sizeof...(BaseNeighborhoodExplorers); }

protected:
  
  typedef ActiveMove<typename BaseNeighborhoodExplorers::ThisMove ...> UnpackedMoves;
  
  /** Constructor, takes a variable number of base NeighborhoodExplorers.  */
  MultimodalNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : SuperNeighborhoodExplorer(in, sm, name),
    nhes(std::make_tuple(nhes ...))
  { }
    
  /**< Instantiated base NeighborhoodExplorers. */
  TheseNeighborhoodExplorers nhes;
  
  /** UnpackAt - run function at a certain level of the tuple */
  template <typename F, typename S, typename ... T>
  void UnpackAt (State& st, std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    if (iter == 0)
      f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves));
    if (iter > 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      UnpackAt(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), --iter);
      temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    }
  }
  
  template <class F>
  void UnpackAt (State& st, std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    if (iter == 0)
      f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }
  
  
  /** UnpackAll - run a function at every element of the tuple */
  template <typename F, typename S, typename ... T>
  void UnpackAll (State& st, std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    // Apply f on head of the tuple
    f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves));
    auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
    UnpackAll(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes));
    temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
  }
  
  template <class F>
  void UnpackAll (State& st, std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }

  
  /** UnpackUntil - run a function until a certain level of a tuple */
  template <typename F, typename S, typename ... T>
  void UnpackUntil(State& st, std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    if (iter == 0)
      f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves));
    if (iter > 0)
    {
      auto tail_temp_moves = TupleUtils::tuple_tail(temp_moves);
      UnpackUntil(st, tail_temp_moves, TupleUtils::tuple_tail(temp_nhes), --iter);
      temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    }
  }
  
  template <class F>
  void UnpackUntil(State& st, std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    if (iter == 0)
      f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }
  
  /** Check - check a predicate on each element of a tuple and returns a vector of the corresponding boolean variable */
  template <typename F, typename S, typename ... T>
  std::vector<bool> Check (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)));
    std::vector<bool> c = Check(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes));
    v.insert(v.end(), c.begin(), c.end());
    return v;
  }
  
  template <class F>
  std::vector<bool> Check (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  const std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhe), st, std::get<0>(temp_move)));
    return v;
  }
  
  /** CheckAnyUntil - check predicate until a certain level of a tuple */
  template <typename F, typename S, typename ... T>
  bool CheckAnyUntil (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)) || (iter > 0 && CheckAnyUntil(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes), --iter));
  }
  
  template <class F>
  bool CheckAnyUntil(const State& st, const std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  const std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    return (iter >= 0) && f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }
  
  /** CheckAllUntil - check predicate until a certain level of a tuple */
  template <typename F, typename S, typename ... T>
  bool CheckAllUntil (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    if (iter > 0)
      return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)) && CheckAllUntil(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes), --iter);
    else
      return true;
  }
  
  template <class F>
  bool CheckAllUntil(const State& st, const std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  const std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    if (iter == 0)
      return f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
    else
      return true;
  }
  
  /** CheckAll - check predicate on all tuple */
  template <typename F, typename S, typename ... T>
  bool CheckAll (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)) && CheckAll(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes));
  }
  
  template <class F>
  bool CheckAll(const State& st, const std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  const std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    return f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }
  
  /** CheckAny - check predicate on all tuple */
  template <typename F, typename S, typename ... T>
  bool CheckAny (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves)) || CheckAny(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes));
  }
  
  template <class F>
  bool CheckAny(const State& st, const std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  const std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f)
  {
    return f(std::get<0>(temp_nhe), st, std::get<0>(temp_move));
  }
  
  /** CheckAt - check predicate at a certain level of a tuple */
  template <typename F, typename S, typename ... T>
  bool CheckAt (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>, ActiveMove<typename S::ThisMove>, ActiveMove<typename T::ThisMove ...>>& temp_moves,  const std::tuple<F, S, T ...> temp_nhes, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    if (iter == 0)
      return f(std::get<0>(temp_nhes), st, std::get<0>(temp_moves));
    if (iter > 0)
      return CheckAt(st, TupleUtils::tuple_tail(temp_moves), TupleUtils::tuple_tail(temp_nhes), --iter);
    return false;
  }
  
  template <class F>
  bool CheckAt (const State& st, const std::tuple<ActiveMove<typename F::ThisMove>>& temp_move,  const std::tuple<F> temp_nhe, std::function<void(F&, State&, ActiveMove<typename F::ThisMove>&)>& f, int iter)
  {
    return (iter == 0 && f(std::get<0>(temp_nhe), st, std::get<0>(temp_move)));
  }
  
  virtual void MakeMove(State& st, const TheseMoves& moves) const
  {
    UnpackAll(st, moves, this->nhes, &DoMakeMove);
  }
  
  virtual bool FeasibleMove(const State& st, const TheseMoves& moves) const
  {
    return CheckAll(st, moves, this->nhes, &IsFeasibleMove);
  }
  
protected:
  
  template<class N, class S, class M>
  bool IsActive(const N& n, const S& s, const M& m)
  {
    return m.active;
  }
  
  template<class N, class S, class M>
  void InitializeInactive(N& n, S& s, M& m)
  {
    m.active = false;
  }
  
  template<class N, class S, class M>
  void DoRandomMove(N& n, S& s, M& m)
  {
    n.RandomMove(s, m);
    m.active = true;
  }
  
  template<class N, class S, class M>
  void DoFirstMove(N& n, S& s, M& m)
  {
    n.FirstMove(s, m);
    m.active = true;
  }
  
  template<class N, class S, class M>
  bool TryNextMove(N& n, S& s, M& m)
  {
    m.active = n.NextMove(s, m);
    return m.active;
  }
  
  template<class N, class S, class M>
  void DoMakeMove(N& n, S& s, M& m)
  {
    if (m.active)
      n.MakeMove(s,m);
  }
  
  template<class N, class S, class M>
  bool IsFeasibleMove(const N& n, const S& s, const M& m)
  {
    if (m.active)
      return m.FeasibleMove(s, m);
    else
      return true;
  }

  
};


template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class SetUnionNeighborhoodExplorer : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>
{
private:
  
  /** Typedefs. */
  typedef typename MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::TheseMoves TheseMoves;
  typedef typename MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::TheseNeighborhoodExplorers TheseNeighborhoodExplorers;
  typedef MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...> SuperNeighborhoodExplorer;


public:
  
  
  /** Inherit constructor from superclass. Not yet, amigo. */
  // using MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::MultimodalNeighborhoodExplorer;
  
  SetUnionNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...)
  { }
  
  virtual void RandomMove(const State& st, TheseMoves& moves) const throw(EmptyNeighborhood)
  {
    int selected = Random::Int(0, this->Modality()-1);
    
    UnpackAll(st, moves, this->nhes, &SuperNeighborhoodExplorer::InitializeInactive);
    UnpackAt(st, moves, this->nhes, &SuperNeighborhoodExplorer::DoRandomMove, selected);
  }
  
  virtual void FirstMove(const State& st, TheseMoves& moves) const throw(EmptyNeighborhood)
  {
    int selected = this->Modality()-1;
    
    UnpackAll(st, moves, this->nhes, &SuperNeighborhoodExplorer::InitializeInactive);
       
    while (selected > 0)
    {
      try
      {
        UnpackAt(st, moves, this->nhes, &SuperNeighborhoodExplorer::DoFirstMove, selected);
        break;
      }
      catch (EmptyNeighborhood e)
      {}
      selected--;
    }
    if (selected < 0)
      throw EmptyNeighborhood();
  }
  
  virtual bool NextMove(const State& st, TheseMoves& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    if (CheckAt(st, moves, this->nhes, &SuperNeighborhoodExplorer::TryNextMove, selected))
      return true;
    
    do
    {
      selected--;
      try
      {
        UnpackAt(st, moves, this->nhes, &SuperNeighborhoodExplorer::DoFirstMove, selected);
        return true;
      }
      catch (EmptyNeighborhood e)
      { }
    } while (selected > 0);
    
    return false;
  }
  
  virtual CFtype DeltaCostFunction(const State& st, const TheseMoves& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    
  }
  
protected:
  int CurrentActiveMove(const State& st, const TheseMoves& moves) const
  {
    std::vector<bool> is_active = this->CheckAllMoves(st, moves, this->nhes, &SuperNeighborhoodExplorer::IsActive);
    return std::distance(is_active.begin(), std::find_if(is_active.begin(), is_active.end(), [](bool& element) { return element; }));
  }
  
};


#endif