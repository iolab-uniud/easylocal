#if !defined(_MULTIMODAL_TABU_LIST_MANAGER_HH_)
#define _MULTIMODAL_TABU_LIST_MANAGER_HH_

#include <helpers/TabuListManager.hh>
#include <helpers/MultimodalNeighborhoodExplorer.hh>

template <class State, typename CFtype, class ... BaseTabuListManagers>
class MultimodalTabuListManager : public TabuListManager<State, std::tuple<ActiveMove<typename BaseTabuListManagers::ThisMove> ...>, CFtype>
{
public:
  /** Typedefs. */
  typedef std::tuple<ActiveMove<typename BaseTabuListManagers::ThisMove> ...> TheseMoves;
  typedef TabuListManager<State, std::tuple<ActiveMove<typename BaseTabuListManagers::ThisMove> ...>, CFtype> SuperTabuListManager;
  typedef std::tuple<std::reference_wrapper<BaseTabuListManagers> ...> TheseTabuListManagers;
  
  /** Modality of the TabuListManager */
  virtual unsigned int Modality() const { return sizeof...(BaseTabuListManagers); }
  
protected:
  /** Constructor, takes a variable number of base TabuListManagers.  */
  MultimodalTabuListManager(BaseTabuListManagers& ... tlms)
  : tlms(std::make_tuple(std::reference_wrapper<BaseTabuListManagers>(tlms) ...))
  {}

  
  TheseTabuListManagers tlms;
  
  /** Tuple dispatching helpers */
  
  class Call
  {
  public:
    enum Function { is_inverse, is_active };
    Call(Function f) : to_call(f) { }
    
    
    template <class T, class M>
    std::function<bool(const T&, const M&, const M&)> getBool() const throw(std::logic_error)
    {
      std::function<bool(const T&, const M&, const M&)> f;
      switch (to_call)
      {
        case is_inverse:
          f = &IsInverse<T, M>;
          break;
        case is_active:
          f = &IsActive<T, M>;
          break;
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }
        
    Function to_call;
  };
  
  template <class TupleOfMoves, class TupleOfTLMs, std::size_t N>
  struct TupleDispatcher
  {
    static std::vector<bool> Check(State& st, const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
    {
      typedef typename std::tuple_element<N, TupleOfMoves>::type CurrentMove;
      typedef typename std::tuple_element<N, TupleOfTLMs>::type CurrentTLM;
      
      const CurrentTLM& this_tlm = std::get<N>(temp_tlms);
      const CurrentMove& this_move_1 = std::get<N>(temp_moves_1);
      const CurrentMove& this_move_2 = std::get<N>(temp_moves_2);
      
      std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();
      
      std::vector<bool> current, others;
      current.push_back(f(this_tlm, this_move_1, this_move_2));
      others = TupleDispatcher<TupleOfMoves, TupleOfTLMs, N - 1>::Check(st, temp_moves_1, temp_moves_2, temp_tlms, c);
      current.insert(current.begin(), others.begin(), others.end());
      return current;
    }
    
    static bool CheckAll(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
    {
      typedef typename std::tuple_element<N, TupleOfMoves>::type CurrentMove;
      typedef typename std::tuple_element<N, TupleOfTLMs>::type CurrentTLM;
      
      const CurrentTLM& this_tlm = std::get<N>(temp_tlms);
      const CurrentMove& this_move_1 = std::get<N>(temp_moves_1);
      const CurrentMove& this_move_2 = std::get<N>(temp_moves_2);
      
      std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();
      
      if (!f(this_tlm, this_move_1, this_move_2))
        return false;
      
      return TupleDispatcher<TupleOfMoves, TupleOfTLMs, N - 1>::CheckAll(temp_moves_1, temp_moves_2, temp_tlms, c);
    }
    
    static bool CheckAny(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
    {
      typedef typename std::tuple_element<N, TupleOfMoves>::type CurrentMove;
      typedef typename std::tuple_element<N, TupleOfTLMs>::type CurrentTLM;
      
      const CurrentTLM& this_tlm = std::get<N>(temp_tlms);
      const CurrentMove& this_move_1 = std::get<N>(temp_moves_1);
      const CurrentMove& this_move_2 = std::get<N>(temp_moves_2);
      
      std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();

      
      if (f(this_tlm, this_move_1, this_move_2))
        return true;
      
      return TupleDispatcher<TupleOfMoves, TupleOfTLMs, N - 1>::CheckAny(temp_moves_1, temp_moves_2, temp_tlms, c);
    }
    
    static bool CheckAt(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c, int iter)
    {
      if (iter == 0)
      {
        typedef typename std::tuple_element<N, TupleOfMoves>::type CurrentMove;
        typedef typename std::tuple_element<N, TupleOfTLMs>::type CurrentTLM;
        
        const CurrentTLM& this_tlm = std::get<N>(temp_tlms);
        const CurrentMove& this_move_1 = std::get<N>(temp_moves_1);
        const CurrentMove& this_move_2 = std::get<N>(temp_moves_2);
        
        std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();
        return f(this_tlm, this_move_1, this_move_2);
      }
      else if (iter > 0)
        return TupleDispatcher<TupleOfMoves, TupleOfTLMs, N - 1>::CheckAt(temp_moves_1, temp_moves_2, temp_tlms, c, --iter);
#if defined(DEBUG)
      else
        throw std::logic_error("In function CheckAt (recursive case) iter is less than zero");
#endif
      return false;
    }
  };
  
  template <class TupleOfMoves, class TupleOfTLMs>
  struct TupleDispatcher<TupleOfMoves, TupleOfTLMs, 0>
  {
    static std::vector<bool> Check(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
    {
      typedef typename std::tuple_element<0, TupleOfMoves>::type CurrentMove;
      typedef typename std::tuple_element<0, TupleOfTLMs>::type CurrentTLM;
      
      const CurrentTLM& this_tlm = std::get<0>(temp_tlms);
      const CurrentMove& this_move_1 = std::get<0>(temp_moves_1);
      const CurrentMove& this_move_2 = std::get<0>(temp_moves_2);
      
      std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();
      std::vector<bool> current;
      current.push_back(f(this_tlm, this_move_1, this_move_2));
      return current;
    }
    
    static bool CheckAll(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
    {
      typedef typename std::tuple_element<0, TupleOfMoves>::type CurrentMove;
      typedef typename std::tuple_element<0, TupleOfTLMs>::type CurrentTLM;
      
      const CurrentTLM& this_tlm = std::get<0>(temp_tlms);
      const CurrentMove& this_move_1 = std::get<0>(temp_moves_1);
      const CurrentMove& this_move_2 = std::get<0>(temp_moves_2);
      
      std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();
      return f(this_tlm, this_move_1, this_move_2);
    }
    
    static bool CheckAny(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
    {
      typedef typename std::tuple_element<0, TupleOfMoves>::type CurrentMove;
      typedef typename std::tuple_element<0, TupleOfTLMs>::type CurrentTLM;
      
      const CurrentTLM& this_tlm = std::get<0>(temp_tlms);
      const CurrentMove& this_move_1 = std::get<0>(temp_moves_1);
      const CurrentMove& this_move_2 = std::get<0>(temp_moves_2);
      
      std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();
      return f(this_tlm, this_move_1, this_move_2);
    }
    
    static bool CheckAt(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c, int iter)
    {
      if (iter == 0)
      {
        typedef typename std::tuple_element<0, TupleOfMoves>::type CurrentMove;
        typedef typename std::tuple_element<0, TupleOfTLMs>::type CurrentTLM;
        
        const CurrentTLM& this_tlm = std::get<0>(temp_tlms);
        const CurrentMove& this_move_1 = std::get<0>(temp_moves_1);
        const CurrentMove& this_move_2 = std::get<0>(temp_moves_2);
        
        std::function<bool(const CurrentTLM&, const CurrentMove&, const CurrentMove&)> f =  c.template getBool<CurrentTLM,CurrentMove>();
        return f(this_tlm, this_move_1, this_move_2);
      }
#if defined(DEBUG)
      else
        throw std::logic_error("In function CheckAt (base case) iter is not zero");
#endif
      return false;
    }
  };
    
  /** Check - checks a predicate on each element of a tuple and returns a vector of the corresponding boolean variable */
  template <class ...TLMs>
  static std::vector<bool> Check(const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_2, const std::tuple<std::reference_wrapper<TLMs>...>& tlms, const Call& c)
  {
    return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::ThisMove>...>, std::tuple<std::reference_wrapper<TLMs>...>, sizeof...(TLMs) - 1>::Check(moves_1, moves_2, tlms, c);
  }
  
  /** CheckAll - checks a predicate on all tuple */
  template <class ...TLMs>
  static bool CheckAll(const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_2, const std::tuple<std::reference_wrapper<TLMs>...>& tlms, const Call& c)
  {
    return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::ThisMove>...>, std::tuple<std::reference_wrapper<TLMs>...>, sizeof...(TLMs) - 1>::CheckAll(moves_1, moves_2, tlms, c);
  }
  
  /** CheckAny - checks a predicate on all tuple */
  template <class ...TLMs>
  static bool CheckAny(const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_2, const std::tuple<std::reference_wrapper<TLMs>...>& tlms, const Call& c)
  {
    return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::ThisMove>...>, std::tuple<std::reference_wrapper<TLMs>...>, sizeof...(TLMs) - 1>::CheckAny(moves_1, moves_2, tlms, c);
  }
  
  /** CheckAt - checks a predicate at a certain level of the tuple and returns its value */
  template <class ...TLMs>
  static CFtype CheckAt(const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::ThisMove>...>& moves_2, const std::tuple<std::reference_wrapper<TLMs>...>& tlms, const Call& c, int iter)
  {
    return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::ThisMove>...>, std::tuple<std::reference_wrapper<TLMs>...>, sizeof...(TLMs) - 1>::CheckAt(moves_1, moves_2, tlms, c, (sizeof...(TLMs) - 1) - iter);
  }
  
  template <class T, class M>
  static bool IsInverse(const T& tlm, const M& move_1, const M& move_2)
  {
    return (move_1.active && move_2.active && tlm.get().Inverse(move_1, move_2));
  }
  
  template <class T, class M>
  static bool IsActive(const T& tlm, const M& move_1, const M& move_2)
  {
    return move_1.active;
  }
};

template <class State, typename CFtype, class ... BaseTabuListManagers>
class SetUnionTabuListManager : MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>
{
  typedef MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...> SuperTabuListManager;
  typedef typename SuperTabuListManager::Call Call;
public:
  SetUnionTabuListManager(BaseTabuListManagers& ... tlms)
  : MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>(tlms...)
  {}
  virtual bool Inverse(const typename SuperTabuListManager::ThisMove& moves_1, const typename SuperTabuListManager::ThisMove& moves_2) const
  {
    Call is_inverse(Call::Function::is_inverse);
    return SuperTabuListManager::CheckAny(moves_1, moves_2, this->tlms, is_inverse);
  }
};

template <class State, typename CFtype, class ... BaseTabuListManagers>
class CartesianProductTabuListManager : MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>
{
  typedef MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...> SuperTabuListManager;
  typedef typename SuperTabuListManager::Call Call;
public:
  CartesianProductTabuListManager(BaseTabuListManagers& ... tlms)
  : MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>(tlms...)
  {}
  virtual bool Inverse(const typename SuperTabuListManager::ThisMove& moves_1, const typename SuperTabuListManager::ThisMove& moves_2) const
  {
    Call is_inverse(Call::Function::is_inverse);
    return SuperTabuListManager::CheckAll(moves_1, moves_2, this->tlms, is_inverse);
  }
};

#endif // _MULTIMODAL_TABU_LIST_MANAGER_HH_
