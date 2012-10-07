#if !defined(_MULTIMODALNEIGHBORHOOD_EXPLORER_HH_)
#define _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_

#include <helpers/StateManager.hh>
#include <helpers/ProhibitionManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <utils/Tuple.hh>
#include <stdexcept>
#include <functional>
#include <algorithm>

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
class MultimodalNeighborhoodExplorer
: public NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::ThisMove> ...>, CFtype>
{
protected:

  /** Struct to encapsulate a templated function call. Since the access to tuples must be done through compile-time recursion, at the moment of calling BaseNeighborhoodExplorers methods we don't know yet the type of the current NeighborhoodExplorer and move. This class is necessary so that we can defer the instantiation of the templated helper functions (DoMakeMove, DoRandomMove, ...) inside the calls to Compute* and Execute* (when they are known). */
  struct Call
  {
    /** An enum representing the functions which can be called. */
    enum Function
    {
      MAKE_MOVE,
      IS_FEASIBLE_MOVE,
      INITIALIZE_INACTIVE,
      RANDOM_MOVE,
      FIRST_MOVE,
      TRY_NEXT_MOVE,
      IS_ACTIVE,
      DELTA_COST_FUNCTION
    };
    
    /** Constructor. 
     @param f the Function to call. */
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
          f = &DoInitializeInactive<N, M>;
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
        case IS_FEASIBLE_MOVE:
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
    
  /** Constructor, takes a variable number of base NeighborhoodExplorers.  */
  MultimodalNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
  : SuperNeighborhoodExplorer(in, sm, name),
    nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes) ...))
  { }
    
  /**< Instantiated base NeighborhoodExplorers. */
  TheseNeighborhoodExplorers nhes;
  
protected:
  
  /** Run a function on a certain element of the Moves (and NeighborhoodExplorers) tuple, then update the element. Based on compile-time recursion.
   @param st reference to the current state
   @param temp_moves tuple of references to ActiveMoves
   @param temp_nhes tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <typename H, typename ... T>
  static void ExecuteAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int level)
  {
    // If we're at the right level of the recursion, instantiate the function and run it on the head of the tuples
    if (level == 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    // Otherwise, go down with recursion
    if (level > 0)
    {
      auto tail_temp_moves = tuple_tail(temp_moves);
      ExecuteAt(st, tail_temp_moves, tuple_tail(temp_nhes), c, --level);
      
      // Write (potential) result on passed tuple of moves
      temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
    }
  }

  /** Run a function on a certain element of the Moves (and NeighborhoodExplorers) tuple, then update the element. Based on compile-time recursion, base case.
   @param st reference to the current state
   @param temp_move singleton tuple of references to ActiveMoves
   @param temp_nhe single tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */  template <class H>
  static void ExecuteAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int level)
  {
    // If we're at the right level of the recursion, instantiate the function and run it on the only element in the tuple
    if (level == 0)
    {
      std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
      f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
  }


  /** Run a function on all the elements of the Moves (and NeighborhoodExplorers) tuple, then update them. Based on compile-time recursion.
   @param st reference to the current state
   @param temp_moves tuple of references to ActiveMoves
   @param temp_nhes tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <typename H, typename ... T>
  static void ExecuteAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove>...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    // Instantiate the function with the right template parameters
    std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
    
    // Run it on the head of the tuples and then go down with recursion
    f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    auto tail_temp_moves = tuple_tail(temp_moves);
    ExecuteAll(st, tail_temp_moves, tuple_tail(temp_nhes), c);
    
    // Merge the results and write them in the original tuples
    temp_moves = std::tuple_cat(std::make_tuple(std::get<0>(temp_moves)), tail_temp_moves);
  }

  /** Run a function on all the elements of the Moves (and NeighborhoodExplorers) tuple, then update them. Based on compile-time recursion, base case.
   @param st reference to the current state
   @param temp_move singleton tuple of references to ActiveMoves
   @param temp_nhe singleton tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <class H>
  static void ExecuteAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    // Instantiate the function with the right template parameters
    std::function<void(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getVoid<H,ActiveMove<typename H::ThisMove>>();
    
    // Run the function on the only element of the tuple (overwrites original data)
    f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
  }
  
  
  /** Run a function on all the elements of the Moves (and NeighborhoodExplorers) tuple, then update them and returns the result. Based on compile-time recursion.
   @param st reference to the current state
   @param temp_moves tuple of references to ActiveMoves
   @param temp_nhes tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <typename H, typename ... T>
  static CFtype ComputeAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int iter)
  {
    // If this is the right level of the recursion, instantiate the function with the right template, then run it on the head of the recursion
    if (iter == 0)
    {
      std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    // Otherwise, go down with recursion
    if (iter > 0)
    {
      auto tail_temp_moves = tuple_tail(temp_moves);
      return ComputeAt(st, tail_temp_moves, tuple_tail(temp_nhes), c, --iter);
    }
    // Just to prevent warning from smart compilers
    return static_cast<CFtype>(0);
  }
  
  /** Run a function on all the elements of the Moves (and NeighborhoodExplorers) tuple, then update them and returns the result. Based on compile-time recursion, base case.
   @param st reference to the current state
   @param temp_move singleton tuple of references to ActiveMoves
   @param temp_nhe singleton tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <class H>
  static CFtype ComputeAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int iter)
  {
    // If this is the right level of the recursion, instantiate the function with the right template parameters and run it on the only element of the tuple
    if (iter == 0)
    {
      std::function<CFtype(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getCFtype<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
    // Just to prevent warning from smart compilers
    return static_cast<CFtype>(0);
  }

  /** Check a predicate on each element of a Moves (and NeighborhoodExplorers) tuple and returns a vector of the corresponding boolean results. Based on compile-time recursion.
   @param st reference to the current state
   @param temp_moves tuple of references to ActiveMoves
   @param temp_nhes tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <typename H, typename ... T>
  static std::vector<bool> Check (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves,  const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    // Instantiate the function with the right template parameters
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get()));
    
    // Go down with recursion
    auto tail_temp_moves = tuple_tail(temp_moves);
    std::vector<bool> ch = Check(st, tail_temp_moves, tuple_tail(temp_nhes), c);
    v.insert(v.end(), ch.begin(), ch.end());
    return v;
  }
  
  /** Check a predicate on each element of a Moves (and NeighborhoodExplorers) tuple and returns a vector of the corresponding boolean results. Based on compile-time recursion, base case.
   @param st reference to the current state
   @param temp_move singleton tuple of references to ActiveMoves
   @param temp_nhe singleton tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <class H>
  static std::vector<bool> Check (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move,  const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    // Instantiate and run the function on the only element of the tuple and return the result wrapped in a vector of bool
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    std::vector<bool> v;
    v.push_back(f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get()));
    return v;
  }
  
  /** Check a predicate on all the elements of a Moves (and NeighborhoodExplorers) tuple and returns true if the predicate is satisfied for every element. Based on compile-time recursion.
   @param st reference to the current state
   @param temp_moves tuple of references to ActiveMoves
   @param temp_nhes tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <typename H, typename ... T>
  static bool CheckAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    // Check predicate on the head of the tuple 
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    if (!f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get()))
      return false;
    
    // If the predicate is satisfied, go down with recursion
    auto tail_temp_moves = tuple_tail(temp_moves);
    return CheckAll(st, tail_temp_moves, tuple_tail(temp_nhes), c);
  }
  
  /** Check a predicate on all the elements of a Moves (and NeighborhoodExplorers) tuple and returns true if the predicate is satisfied for every element. Based on compile-time recursion, base case.
   @param st reference to the current state
   @param temp_move singleton tuple of references to ActiveMoves
   @param temp_nhe singleton tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <class H>
  static bool CheckAll (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    // Check predicate on the only element of the tuple
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
  }
  
  /** Check a predicate on all the elements of a Moves (and NeighborhoodExplorers) tuple and returns true if the predicate is satisfied for at least one element. Based on compile-time recursion.
   @param st reference to the current state
   @param temp_moves tuple of references to ActiveMoves
   @param temp_nhes tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <typename H, typename ... T>
  static bool CheckAny (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_moves, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c)
  {
    // Check predicate on the head of the tuple
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    if (f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get()))
      return true;
    
    // If predicate is false, get down with recursion
    auto tail_temp_moves = tuple_tail(temp_moves);
    return CheckAll(st, tail_temp_moves, tuple_tail(temp_nhes), c);
  }
  
  /** Check a predicate on all the elements of a Moves (and NeighborhoodExplorers) tuple and returns true if the predicate is satisfied for at least one element. Based on compile-time recursion, base case.
   @param st reference to the current state
   @param temp_move singleton tuple of references to ActiveMoves
   @param temp_nhe singleton tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <class H>
  static bool CheckAny (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move, const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c)
  {
    // Check predicate on the only element of the tuple
    std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f = c.template getBool<H,ActiveMove<typename H::ThisMove>>();
    return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
  }
  
  /** Check a predicate at a certian element of the Moves (and NeighborhoodExplorers) tuple and returns true if the predicate is satisfied. Based on compile-time recursion.  
   @param st reference to the current state
   @param temp_moves tuple of references to ActiveMoves
   @param temp_nhes tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <typename H, typename ... T>
  static bool CheckAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>, std::reference_wrapper<ActiveMove<typename T::ThisMove> ...>>& temp_move,  const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T> ...> temp_nhes, const Call& c, int iter)
  {
    // If we're at the right level of the recursion instantiate the function with the right template parameters, then run it on the head of the tuple
    if (iter == 0)
    {
      std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhes).get(), st, std::get<0>(temp_moves).get());
    }
    // Otherwise go down with recursion
    if (iter > 0)
    {
      auto tail_temp_moves = tuple_tail(temp_moves);
      return CheckAt(st, tail_temp_moves, tuple_tail(temp_nhes), c, --iter);
    }
    // Just to prevent warning from smart compilers
    return false;
  }
  
  /** Check a predicate at a certian element of the Moves (and NeighborhoodExplorers) tuple and returns true if the predicate is satisfied. Based on compile-time recursion, base case.
   @param st reference to the current state
   @param temp_move singleton tuple of references to ActiveMoves
   @param temp_nhe singleton tuple of references to associated NeighborhoodExplorers
   @param level level at which to execute the function
   */
  template <class H>
  static bool CheckAt (State& st, std::tuple<std::reference_wrapper<ActiveMove<typename H::ThisMove>>>& temp_move,  const std::tuple<std::reference_wrapper<H>> temp_nhe, const Call& c, int iter)
  {
    // If we're at the right level of the recursion instantiate the function with the right template parameters, then run it on the only element of the tuple
    if (iter == 0)
    {
      std::function<bool(const H&, State&, ActiveMove<typename H::ThisMove>&)> f =  c.template getBool<H,ActiveMove<typename H::ThisMove>>();
      return f(std::get<0>(temp_nhe).get(), st, std::get<0>(temp_move).get());
    }
    // Just to prevent warning from smart compilers
    return false;
  }

  /** Perform a move on the state. Common to all multimodal NeighborhoodExplorer.
   @param st reference to the current State
   @param moves tuple of moves of this multimodal NeighborhoodExplorer 
   */
  virtual void MakeMove(State& st, const TheseMoves& moves) const
  {
    Call make_move(Call::Function::MAKE_MOVE);
    TheseMovesRefs r_moves = const_cast<TheseMoves&>(moves);
    ExecuteAll(st, r_moves, this->nhes, make_move);
  }
  
  /** Checks if a move is feasible from a state. Common to all multimodal NeighborhoodExplorer.
   @param st reference to the current State
   @param moves tuple of moves of this multimodal NeighborhoodExplorer
   */
  virtual bool FeasibleMove(const State& st, const TheseMoves& moves) const
  {
    Call is_feasible_move(Call::Function::IS_FEASIBLE_MOVE);
    TheseMovesRefs r_moves = const_cast<TheseMoves&>(moves);
    return CheckAll(const_cast<State&>(st), r_moves, this->nhes, is_feasible_move);
  }
  
protected:
  
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
  static void DoInitializeInactive(const N& n, State& s, M& m)
  {
    m.active = false;
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
    
    int selected = this->Modality()-1;
    
    TheseMovesRefs r_moves = moves;
    SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);
       
    while (selected > 0)
    {
      try
      {
        SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, first_move, selected);
        break;
      }
      catch (EmptyNeighborhood e)
      {}
      selected--;
    }
    if (selected < 0)
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
      selected--;
      if (selected < 0)
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

#endif