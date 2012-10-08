#if !defined(_MULTIMODALNEIGHBORHOOD_EXPLORER_HH_)
#define _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_

#include <helpers/StateManager.hh>
#include <helpers/ProhibitionManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <utils/Tuple.hh>

/** Template class to incapsulate a boolean flag that marks a Move active or inactive in a multi-modal context. */
template <class Move>
class ActiveMove : public Move
{
public:
  bool active;
};

/** Input operator for ActiveMove, calls input operator for Move. */
template <typename Move>
std::istream& operator>>(std::istream& is, ActiveMove<Move>& m)
{
  is >> static_cast<Move&>(m);
  return is;
}

/** Output operator for ActiveMove, calls output operator for Move. */
template <typename Move>
std::ostream& operator<<(std::ostream& os, const ActiveMove<Move>& m)
{
  if (m.active)
    os << static_cast<const Move&>(m);
  return os;
}

/** Variadic multi-modal neighborhood explorer. Generates a NeighborhoodExplorer whose move type is a tuple of ActiveMoves.*/
template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class MultimodalNeighborhoodExplorer : public NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...>, CFtype>
{
public:
  
  /** Tuple type representing the combination of BaseNeighborhoodExplorers' moves. */
  typedef std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...> TheseMoves;
  
  /** Tuple type representing references to BaseNeighborhoodExplorers' moves (because we need to set them). */
  typedef std::tuple<std::reference_wrapper<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove>> ...> TheseMovesRefs;
  
  /** Type of the NeighborhoodExplorer which has a tuple of moves as its move type. */
  typedef NeighborhoodExplorer<Input, State, TheseMoves, CFtype> SuperNeighborhoodExplorer;
  
  /** Tuple type representing references to BaseNeighborhoodExplorers. */
  typedef std::tuple<std::reference_wrapper<BaseNeighborhoodExplorers> ...> TheseNeighborhoodExplorers;
  
  /** Modality of the NeighborhoodExplorer */
  virtual unsigned int Modality() const { return sizeof...(BaseNeighborhoodExplorers); }
  
protected:
  
  /** Constructor, takes a variable number of base NeighborhoodExplorers.  */
  MultimodalNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : SuperNeighborhoodExplorer(in, sm, name),
  nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes) ...))
  { }
  
  /** Instantiated base NeighborhoodExplorers. */
  TheseNeighborhoodExplorers nhes;
    
  /** Struct to encapsulate a templated function call. Since the access to tuples must be done through compile-time recursion, at the moment of calling BaseNeighborhoodExplorers methods we don't know yet the type of the current NeighborhoodExplorer and move. This class is necessary so that we can defer the instantiation of the templated helper functions (DoMakeMove, DoRandomMove, ...) inside the calls to Compute* and Execute* (when they are known). */
  class Call
  {
  public:
    
    /** An enum representing the functions which can be called. */
    enum Function
    {
      INITIALIZE_INACTIVE,
      INITIALIZE_ACTIVE,
      MAKE_MOVE,
      FEASIBLE_MOVE,
      RANDOM_MOVE,
      FIRST_MOVE,
      TRY_NEXT_MOVE,
      IS_ACTIVE,
      DELTA_COST_FUNCTION
    };
    
    /** Constructor.
     @param f the Function to call. 
     */
    Call(Function f) : to_call(f) { }

    /** Method to generate a function with void return type. */
    template <class N, class M>
    std::function<void(const N&, State&, M&)> getVoid() const throw(std::logic_error)
    {
      std::function<void(const N&, State&, M&)> f;
      switch (to_call)
      {
        case MAKE_MOVE:
          f = &DoMakeMove<N, M>;
          break;
        case INITIALIZE_INACTIVE:
          f = &InitializeInactive<N, M>;
          break;
        case INITIALIZE_ACTIVE:
          f = &InitializeActive<N, M>;
          break;
        case FIRST_MOVE:
          f = &DoFirstMove<N,M>;
          break;
        case RANDOM_MOVE:
          f = &DoRandomMove<N,M>;
          break;
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }

    /** Method to generate a function with CFtype return type. */
    template <class N, class M>
    std::function<CFtype(const N&, State&, M&)> getCFtype() const throw(std::logic_error)
    {
      std::function<CFtype(const N&, State&, M&)> f;
      switch (to_call)
      {
        case DELTA_COST_FUNCTION:
          f = &DoDeltaCostFunction<N,M>;
          break;
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }

    /** Method to generate a function with bool return type. */
    template <class N, class M>
    std::function<bool(const N&, State&, M&)> getBool() const throw(std::logic_error)
    {
      std::function<bool(const N&, State&, M&)> f;
      switch (to_call)
      {
        case FEASIBLE_MOVE:
          f = &IsFeasibleMove<N,M>;
          break;
        case TRY_NEXT_MOVE:
          f = &TryNextMove<N,M>;
          break;
        case IS_ACTIVE:
          f = &IsActive<N,M>;
          break;
        default:
          throw std::logic_error("Function not implemented");
      }
      return f;
    }
    
    Function to_call;
  };
  
  /** Tuple dispatcher. Similarly to Call, this struct defines a number of templated functions that accept a tuple of moves and a tuple of neighborhood explorers, together with additional paramters. The struct is template-specialized to handle compile-time recursion.
   */
  template <class TupleOfMoves, class TupleOfNHEs, std::size_t N>
  struct TupleDispatcher
  {
    /** Run a function on a specific element of the tuples. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
    {
      // If we're at the right level of the recursion
      if (level == 0)
      {
        // Get last element of the tuples
        auto& this_nhe = std::get<N>(temp_nhes).get();
        auto& this_move = std::get<N>(temp_moves).get();
        
        // Instantiate the function with the right template parameters
        std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
        
        // Call on the last element
        f(this_nhe, st, this_move);
      }
      // Otherwise go down with recursion
      else if (level > 0)
        TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ExecuteAt(st, temp_moves, temp_nhes, c, --level);
#if defined(DEBUG)
      else
        throw std::logic_error("In function ExecuteAt (recursive case) level is less than zero");
#endif
    }
    
    /** Run a function on all the elements of the Moves (and NeighborhoodExplorers) tuple. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static void ExecuteAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get last element of the tuples
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
      
      // Call recursively on first N-1 elements of the tuple, then call on the last element of the tuple
      TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ExecuteAll(st, temp_moves, temp_nhes, c);
      f(this_nhe, st, this_move);
    }
    
    /** Run a function on all the elements of tuples, then return the result. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static CFtype ComputeAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
    {
      // If we're at the right level of the recursion
      if (level == 0)
      {
        // Get last element of the tuples
        auto& this_nhe = std::get<N>(temp_nhes).get();
        auto& this_move = std::get<N>(temp_moves).get();
        
        // Instantiate the function with the right template parameters
        std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
        
        // Call on the last element, return result
        return f(this_nhe, st, this_move);
      }
      // Otherwise, go down with recursion
      else if (level > 0)
        return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ComputeAt(st, temp_moves, temp_nhes, c, --level);
#if defined(DEBUG)
      else
        throw std::logic_error("In function ComputeAt (recursive case) level is less than zero");
#endif
      // Just to avoid warnings from smart compilers
      return static_cast<CFtype>(0);
    }
    
    /** Run a function at all the levels of tuples, then return the sum of the result. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static CFtype ComputeAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get last element of the tuples
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
      
      // Return aggregate result
      return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ComputeAll(st, temp_moves, temp_nhes, c) + f(this_nhe, st, this_move);
    }

    /** Check a predicate on each element of the tuples and returns the vector of the corresponding boolean results. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static std::vector<bool> Check(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get last element of the tuple
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();

      // Go down with recursion and merge results
      std::vector<bool> current, others;
      current.push_back(f(this_nhe, st, this_move));
      others = TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::Check(st, temp_moves, temp_nhes, c);
      current.insert(current.begin(), others.begin(), others.end());
      return current;
    }

    /** Check a predicate on each element of the tuples and returns true if all of them satisfies the predicate. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static bool CheckAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get last element of the tuple
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      
      // Lazy evaluation
      if (!f(this_nhe, st, this_move))
        return false;
      
      return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAll(st, temp_moves, temp_nhes, c);
    }
    
    /** Check a predicate on each element of the tuples and returns true if at least one of them satisfies the predicate. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static bool CheckAny(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get last element of the tuple
      auto& this_nhe = std::get<N>(temp_nhes).get();
      auto& this_move = std::get<N>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      
      // Lazy evaluation
      if (f(this_nhe, st, this_move))
        return true;
      
      return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAll(st, temp_moves, temp_nhes, c);
    }

    /** Check a predicate on a specific element of the tuples and returns true if it satisfies the predicate. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
    {
      // If we're at the right level of the recursion
      if (level == 0)
      {
        // Get the last element of the tuples
        auto& this_nhe = std::get<N>(temp_nhes).get();
        auto& this_move = std::get<N>(temp_moves).get();
        
        // Instantiate the function with the right template parameters and return the result
        std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
        return f(this_nhe, st, this_move);
      }
      // Otherwise, go down with recursion
      else if (level > 0)
        return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAt(st, temp_moves, temp_nhes, c, --level);
#if defined(DEBUG)
      else
        throw std::logic_error("In function CheckAt (recursive case) level is less than zero");
#endif
      // Just to avoid warnings from smart compilers
      return false;
    }
  };
  
  /** Base case of the recursive TupleDispatcher. */
  template <class TupleOfMoves, class TupleOfNHEs>
  struct TupleDispatcher<TupleOfMoves, TupleOfNHEs, 0>
  {
    /** Run a function on a specific element of the tuples. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
    {
      // If we're at the right level of the recursion
      if (level == 0)
      {
        // Get the last element of the tuples
        auto& this_nhe = std::get<0>(temp_nhes).get();
        auto& this_move = std::get<0>(temp_moves).get();
        
        // Instantiate the function with the right template parameters
        std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
        f(this_nhe, st, this_move);
      }
#if defined(DEBUG)
      else
        throw std::logic_error("In function ExecuteAt (base case) level is not zero");
#endif
    }

    /** Run a function on all the elements of the Moves (and NeighborhoodExplorers) tuple. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static void ExecuteAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get the last element of the tuples
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<void(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getVoid<decltype(this_nhe),decltype(this_move)>();
      f(this_nhe, st, this_move);
    }
    
    /** Run a function on all the elements of tuples, then return the result. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static CFtype ComputeAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
    {
      // If we're at the right level of the recursion
      if (level == 0)
      {
        // Get the last element of the tuples
        auto& this_nhe = std::get<0>(temp_nhes).get();
        auto& this_move = std::get<0>(temp_moves).get();
        
        // Instantiate the function with the right template parameters
        std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
        return f(this_nhe, st, this_move);
      }
#if defined(DEBUG)
      else
        throw std::logic_error("In function ComputeAt (base case) level is not zero");
#endif
      // Just to avoid warnings from smart compilers
      return static_cast<CFtype>(0);
    }
    
    /** Run a function at all the levels of tuples, then return the sum of the result. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static CFtype ComputeAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get the last element of the tuples
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<CFtype(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getCFtype<decltype(this_nhe),decltype(this_move)>();
      return f(this_nhe, st, this_move);
    }

    /** Check a predicate on each element of the tuples and returns the vector of the corresponding boolean results. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static std::vector<bool> Check(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get the last element of the tuples
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      std::vector<bool> current;
      current.push_back(f(this_nhe, st, this_move));
      return current;
    }
    
    /** Check a predicate on each element of the tuples and returns true if all of them satisfies the predicate. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static bool CheckAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get the last element of the tuples
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      return f(this_nhe, st, this_move);
    }

    /** Check a predicate on each element of the tuples and returns true if at least one of them satisfies the predicate. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static bool CheckAny(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
    {
      // Get the last element of the tuples
      auto& this_nhe = std::get<0>(temp_nhes).get();
      auto& this_move = std::get<0>(temp_moves).get();
      
      // Instantiate the function with the right template parameters
      std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
      return f(this_nhe, st, this_move);
    }

    /** Check a predicate on a specific element of the tuples and returns true if it satisfies the predicate. Based on compile-time recursion.
     @param st reference to the current state
     @param temp_moves tuple of references to ActiveMoves
     @param temp_nhes tuple of references to associated NeighborhoodExplorers
     @param level level at which to execute the function
     */
    static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
    {
      // If we're at the right level of the recursion
      if (level == 0)
      {
        // Get the last element of the tuples
        auto& this_nhe = std::get<0>(temp_nhes).get();
        auto& this_move = std::get<0>(temp_moves).get();
        
        // Instantiate the function with the right template parameters
        std::function<bool(const decltype(this_nhe)&, State&, decltype(this_move)&)> f =  c.template getBool<decltype(this_nhe),decltype(this_move)>();
        return f(this_nhe, st, this_move);
      }
#if defined(DEBUG)
      else
        throw std::logic_error("In function CheckAt (base case) level is not zero");
#endif
      // Just to avoid warnings from smart compilers
      return false;
    }
  };
  
  /** @copydoc TupleDispatcher::ExecuteAt */
  template <class ...NHEs>
  static void ExecuteAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                        const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int level)
  {
    TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ExecuteAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - level);
  }
  
  /** @copydoc TupleDispatcher::ExecuteAll */
  template <class ...NHEs>
  static void ExecuteAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                         const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ExecuteAll(st, moves, nhes, c);
  }
  
  /** @copydoc TupleDispatcher::ComputeAt */
  template <class ...NHEs>
  static CFtype ComputeAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                          const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int level)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ComputeAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - level);
  }
  
  /** @copydoc TupleDispatcher::ComputeAll */
  template <class ...NHEs>
  static CFtype ComputeAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                           const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ComputeAll(st, moves, nhes, c);
  }
  
  /** @copydoc TupleDispatcher::Check */
  template <class ...NHEs>
  static std::vector<bool> Check(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                                 const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::Check(st, moves, nhes, c);
  }
  
  /** @copydoc TupleDispatcher::CheckAll */
  template <class ...NHEs>
  static bool CheckAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                       const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAll(st, moves, nhes, c);
  }
  
  /** @copydoc TupleDispatcher::CheckAny */
  template <class ...NHEs>
  static bool CheckAny(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                       const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAny(st, moves, nhes, c);
  }
  
  /** @copydoc TupleDispatcher::CheckAt */
  template <class ...NHEs>
  static CFtype CheckAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>& moves,
                        const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int level)
  {
    return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::ThisMove>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - level);
  }
  
  /** Checks if a move is active.
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the ActiveMove to check
   */
  template<class N, class M>
  static bool IsActive(const N& n, State& s, M& m)
  {
    return m.active;
  }

  /** Generates a random move in the neighborhood.
   @param n a reference to the NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the ActiveMove which must be set
   */
  template<class N, class M>
  static void DoRandomMove(const N& n, State& s, M& m)
  {
    n.RandomMove(s, m);
    m.active = true;
  }

  /** Get the first move of the neighborhood
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the ActiveMove which must be set
   */
  template<class N, class M>
  static void DoFirstMove(const N& n, State& s, M& m)
  {
    n.FirstMove(s, m);
    m.active = true;
  }

  /** Try to produce the next move of the neighborhood
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the current move, where the next move will be written
   */
  template<class N, class M>
  static bool TryNextMove(const N& n, State& s, M& m)
  {
    M& mm = const_cast<M&>(m);
    mm.active = n.NextMove(s, mm);
    return mm.active;
  }
  
  /** Executes the move on the state, modifies it
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the current move
   */
  template<class N, class M>
  static void DoMakeMove(const N& n, State& s, M& m)
  {
    if (m.active)
      n.MakeMove(s,m);
  }

  /** Checks if a move is feasible.
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the ActiveMove to check
   */
  template<class N, class M>
  static bool IsFeasibleMove(const N& n, State& s, M& m)
  {
    if (m.active)
      return n.FeasibleMove(s, m);
    else
      return true;
  }

  /** Sets the active flag of an ActiveMove to false
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the ActiveMove to check
   */
  template<class N, class M>
  static void InitializeInactive(const N& n, State& s, M& m)
  {
    m.active = false;
  }

  /** Sets the active flag of an ActiveMove to true
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the ActiveMove to check
   */
  template<class N, class M>
  static void InitializeActive(const N& n, State& s, M& m)
  {
    m.active = true;
  }
  
  /** Computes the cost of making a move on a state
   @param n a reference to the move's NeighborhoodExplorer
   @param s a reference to the current State
   @param m a reference to the ActiveMove to check
   */
  template<class N, class M>
  static CFtype DoDeltaCostFunction(const N& n, State& s, M& m)
  {
    return n.DeltaCostFunction(s, m);
  }
};

/** A multi-modal NeighborhoodExplorer that generates the union of multiple neighborhood explorers. */
template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class SetUnionNeighborhoodExplorer : public MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>
{
private:
  
  /** Type of the NeighborhoodExplorer which has a tuple of moves as its move type. */
  typedef MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...> SuperNeighborhoodExplorer;
  
  /** Tuple type representing references to BaseNeighborhoodExplorers' moves (because we need to set them). */
  typedef typename SuperNeighborhoodExplorer::TheseMovesRefs TheseMovesRefs;
  
  /** Alias to SuperNeighborhoodExplorer::Call. */
  typedef typename SuperNeighborhoodExplorer::Call Call;
  
public:
  
  
  /** Inherit constructor from superclass. Not yet. */
  // using MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::MultimodalNeighborhoodExplorer;
  
  SetUnionNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...)
  { }

  /** @copydoc NeighborhoodExplorer::RandomMove */
  virtual void RandomMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    int selected = Random::Int(0, this->Modality()-1);
    
    Call initialize_inactive(Call::Function::INITIALIZE_INACTIVE);
    Call random_move(SuperNeighborhoodExplorer::Call::Function::RANDOM_MOVE);
    
    TheseMovesRefs r_moves = moves;
    SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);
    SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, random_move, selected);
  }
  
  /** @copydoc NeighborhoodExplorer::FirstMove */
  virtual void FirstMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    Call initialize_inactive(Call::Function::INITIALIZE_INACTIVE);
    Call first_move(Call::Function::FIRST_MOVE);
    
    int selected = 0;
    
    TheseMovesRefs r_moves = moves;
    SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);
    
    while (selected < this->Modality())
    {
      try
      {
        SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, first_move, selected);
        break;
      }
      catch (EmptyNeighborhood e)
      {}
      selected++;
    }
    if (selected == this->Modality())
      throw EmptyNeighborhood();
  }
  
  /** @copydoc NeighborhoodExplorer::NextMove */
  virtual bool NextMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    Call try_next_move(Call::Function::TRY_NEXT_MOVE);
    int selected = this->CurrentActiveMove(st, moves);
    TheseMovesRefs r_moves = moves;
    if (SuperNeighborhoodExplorer::CheckAt(const_cast<State&>(st), r_moves, this->nhes, try_next_move, selected))
      return true;
    
    do
    {
      selected++;
      if (selected == this->Modality())
        return false;
      try
      {
        TheseMovesRefs r_moves = moves;
        Call first_move(Call::Function::FIRST_MOVE);
        SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, first_move, selected);
        return true;
      }
      catch (EmptyNeighborhood e)
      { }
    } while (true);
    
    return false;
  }
  
  /** @copydoc NeighborhoodExplorer::DeltaCostFunction */
  virtual CFtype DeltaCostFunction(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call delta_cost_function(Call::Function::DELTA_COST_FUNCTION);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    
    return SuperNeighborhoodExplorer::ComputeAt(const_cast<State&>(st), r_moves, this->nhes, delta_cost_function, selected);
  }

  /** @copydoc NeighborhoodExplorer::MakeMove */
  virtual void MakeMove(State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call make_move(Call::Function::MAKE_MOVE);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    SuperNeighborhoodExplorer::ExecuteAt(st, r_moves, this->nhes, make_move, selected);
  }
  
  /** @copydoc NeighborhoodExplorer::FeasibleMove */
  virtual bool FeasibleMove(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    int selected = this->CurrentActiveMove(st, moves);
    Call feasible_move(Call::Function::FEASIBLE_MOVE);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    return SuperNeighborhoodExplorer::CheckAt(const_cast<State&>(st), r_moves, this->nhes, feasible_move, selected);
  }
  
protected:

  /** Computes the index of the current active move.
   @param st current state
   @param moves tuple of moves
   */
  int CurrentActiveMove(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    Call is_active(Call::Function::IS_ACTIVE);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    std::vector<bool> moves_status = SuperNeighborhoodExplorer::Check(const_cast<State&>(st), r_moves, this->nhes, is_active);
    return std::distance(moves_status.begin(), std::find_if(moves_status.begin(), moves_status.end(), [](bool element) { return element; }));
  }
};

/** A multi-modal NeighborhoodExplorer that generates the cartesian product of multiple neighborhood explorers. */
template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
class CartesianProductNeighborhoodExplorer : public MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>
{
private:
  
  /** Type of the NeighborhoodExplorer which has a tuple of moves as its move type. */
  typedef MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...> SuperNeighborhoodExplorer;
  
  /** Tuple type representing references to BaseNeighborhoodExplorers' moves (because we need to set them). */
  typedef typename SuperNeighborhoodExplorer::TheseMovesRefs TheseMovesRefs;
  
  /** Alias to SuperNeighborhoodExplorer::Call. */
  typedef typename SuperNeighborhoodExplorer::Call Call;
  
#if defined(DEBUG)
  /** Check that all the ActiveMoves, inside a tuple of moves, are active. */
  void VerifyAllActives(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const throw (std::logic_error)
  {
    Call is_active(SuperNeighborhoodExplorer::Call::Function::IS_ACTIVE);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    std::vector<bool> active = SuperNeighborhoodExplorer::Check(const_cast<State&>(st), r_moves, this->nhes, is_active);
    for (bool v : active)
    {
      if (!v)
        throw std::logic_error("Some of the moves were not active in a composite CartesianProduct neighborhood explorer");
    }
  }
#endif
  
public:
  
  
  /** Inherit constructor from superclass. Not yet. */
  // using MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::MultimodalNeighborhoodExplorer;
  
  CartesianProductNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...)
  { }
  
  /** @copydoc NeighborhoodExplorer::RandomMove */
  virtual void RandomMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    Call random_move(SuperNeighborhoodExplorer::Call::Function::RANDOM_MOVE);
    Call make_move(SuperNeighborhoodExplorer::Call::Function::MAKE_MOVE);
    
    TheseMovesRefs r_moves = moves;
    std::vector<State> temp_states(this->Modality() - 1); // the chain of states (including the first one, but excluding the last)
    
    temp_states[0] = st;
    SuperNeighborhoodExplorer::ExecuteAll(temp_states[0], r_moves, this->nhes, random_move);
    SuperNeighborhoodExplorer::ExecuteAt(temp_states[0], r_moves, this->nhes, make_move, 0);
    for (int i = 1; i < this->Modality() - 1; i++)
    {
      temp_states[i] = temp_states[i - 1];
      SuperNeighborhoodExplorer::ExecuteAll(temp_states[i], r_moves, this->nhes, random_move);
      SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, 0);
    }
    SuperNeighborhoodExplorer::ExecuteAt(temp_states[this->Modality() - 2], r_moves, this->nhes, random_move, this->Modality() - 1);
    
    // just for debugging
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
  }
  
  /** @copydoc NeighborhoodExplorer::FirstMove */
  virtual void FirstMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const throw(EmptyNeighborhood)
  {
    Call first_move(Call::Function::FIRST_MOVE);
    Call make_move(Call::Function::MAKE_MOVE);
    Call try_next_move(Call::Function::TRY_NEXT_MOVE);
    
    int i = 0;
    TheseMovesRefs r_moves = moves;
    
    std::vector<State> temp_states(this->Modality(), State(this->in)); // the chain of states
    temp_states[0] = st; // the first state is a copy of to st
    
    // this call order is a small optimization, since first move could fail already on the first state we save a state copy
    SuperNeighborhoodExplorer::ExecuteAt(temp_states[0], r_moves, this->nhes, first_move, 0);
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
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, first_move, i);
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

  /** @copydoc NeighborhoodExplorer::NextMove */
  virtual bool NextMove(const State& st, typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
    Call first_move(Call::Function::FIRST_MOVE);
    Call make_move(Call::Function::MAKE_MOVE);
    Call try_next_move(Call::Function::TRY_NEXT_MOVE);
    
    int i = 0;
    TheseMovesRefs r_moves = moves;
    
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
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, first_move, i);
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
  
  /** @copydoc NeighborhoodExplorer::DeltaCostFunction */
  virtual CFtype DeltaCostFunction(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
    // FIXME: at present is buggy
    Call do_delta_cost_function(Call::Function::DELTA_COST_FUNCTION);
    Call make_move(Call::Function::MAKE_MOVE);
    
    CFtype sum = static_cast<CFtype>(0);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    
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

  /** @copydoc NeighborhoodExplorer::MakeMove */
  virtual void MakeMove(State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
    Call make_move(Call::Function::MAKE_MOVE);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    SuperNeighborhoodExplorer::ExecuteAll(st, r_moves, this->nhes, make_move);
  }
  
  /** @copydoc NeighborhoodExplorer::FeasibleMove */
  virtual bool FeasibleMove(const State& st, const typename SuperNeighborhoodExplorer::ThisMove& moves) const
  {
#if defined(DEBUG)
    VerifyAllActives(st, moves);
#endif
    Call is_feasible_move(Call::Function::FEASIBLE_MOVE);
    Call make_move(Call::Function::MAKE_MOVE);
    TheseMovesRefs r_moves = const_cast<typename SuperNeighborhoodExplorer::ThisMove&>(moves);
    
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