#if !defined(_MULTIMODALNEIGHBORHOOD_EXPLORER_HH_)
#define _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_

#include <stdexcept>
#include <functional>
#include <algorithm>

#include "easylocal/helpers/statemanager.hh"
#include "easylocal/helpers/prohibitionmanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/utils/tuple.hh"

namespace EasyLocal {

  namespace Core {

    /** Template class to incapsulate a boolean flag that marks a @ref Move active or inactive in the context of a @ref MultimodalNeighborhoodExplorer. */
    template <class Move>
    class ActiveMove : public Move
    {
    public:
      bool active;
      
      /** Get the raw @ref Move inside this object. */
      Move& RawMove() 
      { 
        return *this; 
      }
    };

    /** Input operator for @ref ActiveMove, just forwards to input operator for @ref Move.
        @param is input stream, by default @c std::cin
        @param m @ref Move to read
    */
    template <typename Move>
    std::istream& operator>>(std::istream& is, ActiveMove<Move>& m)
    {
      is >> static_cast<Move&>(m);
      return is;
    }

    /** Output operator for @ref ActiveMove, just forwards to output operator for @ref Move. 
        @param os output stream, by default @c std::cout
        @param m @ref Move to print
    */
    template <typename Move>
    std::ostream& operator<<(std::ostream& os, const ActiveMove<Move>& m)
    {
      if (m.active)
        os << static_cast<const Move&>(m);
      return os;
    }

    /** Equality operator for @ref ActiveMove, forwards to equality operator for @ref Move and checks for same @c active flag. 
        @param mv1 first @ref Move
        @param mv2 second @ref Move
        @return true if the two raw @ref Move are the same and the @active flag is the equal
    */
    template <class Move>
    bool operator==(const ActiveMove<Move>& mv1, const ActiveMove<Move>& mv2)
    {
      if (!mv1.active && !mv2.active)
        return true;
      else if (mv1.active != mv2.active)
        return false;
      else
        return static_cast<Move>(mv1) == static_cast<Move>(mv2);
    }
    
    /** Ordering operator for @ActiveMove, forwards to ordering operator for @ref Move and checks for same @c active flag. 
        @param mv1 first @ref Move
        @param mv2 second @ref Move
        @return true if the first @ref Move comes first than the second @ref Move or the first @c active flag is "smaller" than the second
    */
    template <class Move>
    bool operator<(const ActiveMove<Move>& mv1, const ActiveMove<Move>& mv2)
    {
      if (!mv1.active && !mv2.active)
        return false;
      else if (mv1.active < mv2.active)
        return true;
      else if (mv1.active > mv2.active)
        return false;
      else
        return static_cast<Move>(mv1) < static_cast<Move>(mv2);
    }

    /** Helper function to check whether two @ref Move are related. As a general rule all @ref Move are related (unless otherwise specified by overloading this helper).
        @param m1 first @ref Move
        @param m2 second @ref Move
        @remarks the ordering of moves is important
    */
    template <class Move1, class Move2>
    bool IsRelated(const Move1& m1, const Move2& m2)
    {
      return true;
    }

    /** Variadic multi-modal @ref NeighborhoodExplorer. Defines a new @ref NeighborhoodExplorer whose @c MoveType is a tuple of @c ActiveMoves. */
    template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
    class MultimodalNeighborhoodExplorer : public NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType> ...>, CFtype>
    {
    public:

      /** Tuple type representing the combination of @c BaseNeighborhoodExplorers' @ref Move. */
      typedef std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType> ...> MoveTypes;

      /** Tuple type representing references to @c BaseNeighborhoodExplorers' @ref Move (because we need to set them). */
      typedef std::tuple<std::reference_wrapper<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>> ...> MoveTypeRefs;

      /** Type of the @ref NeighborhoodExplorer which has a tuple of @ref Move as its @ref Move type. */
      typedef NeighborhoodExplorer<Input, State, MoveTypes, CFtype> SuperNeighborhoodExplorer;

      /** Tuple type representing references to @c BaseNeighborhoodExplorers. */
      typedef std::tuple<std::reference_wrapper<BaseNeighborhoodExplorers> ...> NeighborhoodExplorerTypes;

      /** Tuple type representing references to @c BaseNeighborhoodExplorers. */
      typedef std::tuple<BaseNeighborhoodExplorers ...> NeighborhoodExplorerConcreteTypes;

      /** Modality of the NeighborhoodExplorer, i.e., the number of @ref NeighborhoodExplorer composing this one.  */
      virtual unsigned int Modality() const { return sizeof...(BaseNeighborhoodExplorers); }

    protected:

      /** Constructor, takes a variable number of base NeighborhoodExplorers.  */
      MultimodalNeighborhoodExplorer(const Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
      : SuperNeighborhoodExplorer(in, sm, name), nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes) ...))
      { }

      /** Instantiated base NeighborhoodExplorers. */
      NeighborhoodExplorerTypes nhes;

      /** Class to encapsulate a templated function call. 
          Since the access to tuples must be done through compile-time recursion, at the moment of calling @c BaseNeighborhoodExplorers methods we don't know yet the type of the current @ref NeighborhoodExplorer and @ref Move. This class is thus necessary so that we can defer the instantiation of the templated helper functions (@c DoMakeMove, @c DoRandomMove, ...) inside the calls to @c Compute* and @c Execute* (when they are known). */
      class Call
      {
      public:

        /** An enum representing the kind of functions which can be called. */
        enum Function
        {
          INITIALIZE_INACTIVE,
          INITIALIZE_ACTIVE,
          MAKE_MOVE,
          RANDOM_MOVE,
          RANDOM_MOVE_WITH_FIRST,
          FIRST_MOVE,
          TRY_NEXT_MOVE,
          TRY_NEXT_MOVE_WITH_FIRST,
          IS_ACTIVE,
          DELTA_COST_FUNCTION,
          DELTA_VIOLATIONS
        };

        /** Constructor.
            @param f the @ref Function to call.
        */
        Call(Function f) : to_call(f) { }

        /** Method to generate a function with @c void return type. */
        template <class N>
        std::function<void(const N&, State&, ActiveMove<typename N::MoveType>&)> getVoid() const throw(std::logic_error)
        {
          std::function<void(const N&, State&, ActiveMove<typename N::MoveType>&)> f;
          switch (to_call)
          {
            case MAKE_MOVE:
            f = &DoMakeMove<N>;
            break;
            case INITIALIZE_INACTIVE:
            f = &InitializeInactive<N>;
            break;
            case INITIALIZE_ACTIVE:
            f = &InitializeActive<N>;
            break;
            case FIRST_MOVE:
            f = &DoFirstMove<N>;
            break;
            case RANDOM_MOVE:
            f = &DoRandomMove<N>;
            break;
            default:
            throw std::logic_error("Function not implemented");
          }
          return f;
        }
        
        /** Method to generate a function with @c void return type. */
        template <class N>
        std::function<void(const N&, State&, ActiveMove<typename N::MoveType>&, ActiveMove<typename N::MoveType>&)> getVoidExt() const throw(std::logic_error)
        {
          std::function<void(const N&, State&, ActiveMove<typename N::MoveType>&, ActiveMove<typename N::MoveType>&)> f;
          switch (to_call)
          {
            case RANDOM_MOVE_WITH_FIRST:
            f = &DoRandomMoveWithFirst<N>;
            break;
            default:
            throw std::logic_error("Function not implemented");
          }
          return f;
        }

        /** Method to generate a function with @c CFtype return type. */
        template <class N>
        std::function<CFtype(const N&, State&, ActiveMove<typename N::MoveType>&)> getCFtype() const throw(std::logic_error)
        {
          std::function<CFtype(const N&, State&, ActiveMove<typename N::MoveType>&)> f;
          switch (to_call)
          {
            case DELTA_COST_FUNCTION:
            f = &DoDeltaCostFunction<N>;
            break;
            case DELTA_VIOLATIONS:
            f = &DoDeltaViolations<N>;
            break;
            default:
            throw std::logic_error("Function not implemented");
          }
          return f;
        }

        /** Method to generate a function with @c bool return type. */
        template <class N>
        std::function<bool(const N&, State&, ActiveMove<typename N::MoveType>&)> getBool() const throw(std::logic_error)
        {
          std::function<bool(const N&, State&, ActiveMove<typename N::MoveType>&)> f;
          switch (to_call)
          {
            case TRY_NEXT_MOVE:
            f = &TryNextMove<N>;
            break;
            case IS_ACTIVE:
            f = &IsActive<N>;
            break;
            default:
            throw std::logic_error("Function not implemented");
          }
          return f;
        }

        /** Method to generate a function with @c bool return type. */
        template <class N>
        std::function<bool(const N&, State&, ActiveMove<typename N::MoveType>&, ActiveMove<typename N::MoveType>&)> getBoolExt() const throw(std::logic_error)
        {
          std::function<bool(const N&, State&, ActiveMove<typename N::MoveType>&, ActiveMove<typename N::MoveType>&)> f;
          switch (to_call)
          {
            case TRY_NEXT_MOVE_WITH_FIRST:
            f = &TryNextMoveWithFirst<N>;
            break;
            default:
            throw std::logic_error("Function not implemented");
          }
          return f;
        }

        Function to_call;
      };

      /** Tuple dispatcher, used to run @ref Call objects on tuples.
          Similarly to @ref Call, this struct defines a number of templated functions that accept a tuple of @ref Move and a tuple of @ref NeighborhoodExplorer, together with other parameters (depending on the function). The struct is template-specialized to handle compile-time recursion. */
      template <class TupleOfMoves, class TupleOfNHEs, std::size_t N>
      struct TupleDispatcher
      {
        

        
        /** Run a function on e specific elements of the @ref Move (and @ref NeighborhoodExplorer) tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, TupleOfMoves& first_moves, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
            CurrentMove& this_move = std::get<N>(temp_moves).get();
            CurrentMove& this_first_move = std::get<N>(first_moves).get();

            // Instantiate the function with the right template parameters
            std::function<void(const CurrentNHE &, State&,  CurrentMove &, CurrentMove& )> f =  c.template getVoidExt<CurrentNHE>();

            // Call on the last element
            f(this_nhe, st, this_move, this_first_move);
          }
          // Otherwise go down with recursion
          else if (level > 0)
            TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ExecuteAt(st, temp_moves, temp_nhes, first_moves, c, --level);
#if defined(DEBUG)
          else
            throw std::logic_error("In function ExecuteAt (recursive case) level is less than zero");
#endif
        }

        
        /** Run a function on e specific elements of the @ref Move (and @ref NeighborhoodExplorer) tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
            CurrentMove& this_move = std::get<N>(temp_moves).get();

            // Instantiate the function with the right template parameters
            std::function<void(const CurrentNHE &, State&,  CurrentMove &)> f =  c.template getVoid<CurrentNHE>();

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

        /** Run a function on all the elements of the @ref Move (and @ref NeighborhoodExplorer) tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static void ExecuteAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
          CurrentMove& this_move = std::get<N>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<void(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getVoid<CurrentNHE>();

          // Call recursively on first N-1 elements of the tuple, then call on the last element of the tuple
          TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ExecuteAll(st, temp_moves, temp_nhes, c);
          f(this_nhe, st, this_move);
        }

        /** Run a function on a specific element of the @ref Move (and @ref NeighborhoodExplorer) tuples, then return the result. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static CFtype ComputeAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
            CurrentMove& this_move = std::get<N>(temp_moves).get();

            // Instantiate the function with the right template parameters
            std::function<CFtype(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getCFtype<CurrentNHE>();

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

        /** Run a function on all the elements of the @ref Move (and @ref NeighborhoodExplorer) tuples, then return the result. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static CFtype ComputeAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
          CurrentMove& this_move = std::get<N>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<CFtype(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getCFtype<CurrentNHE>();

          // Return aggregate result
          return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::ComputeAll(st, temp_moves, temp_nhes, c) + f(this_nhe, st, this_move);
        }

        /** Check a predicate on all the elements of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns the vector of the corresponding boolean results. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static std::vector<bool> Check(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
          CurrentMove& this_move = std::get<N>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<bool(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getBool<CurrentNHE>();

          // Go down with recursion and merge results
          std::vector<bool> current, others;
          current.push_back(f(this_nhe, st, this_move));
          others = TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::Check(st, temp_moves, temp_nhes, c);
          current.insert(current.begin(), others.begin(), others.end());
          return current;
        }

        /** Check a predicate on each element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if all of them satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static bool CheckAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
          CurrentMove& this_move = std::get<N>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<bool(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getBool<CurrentNHE&>();

          // Lazy evaluation
          if (!f(this_nhe, st, this_move))
            return false;

          return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAll(st, temp_moves, temp_nhes, c);
        }

        /** Check a predicate on each element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if at least one of them satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static bool CheckAny(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
          CurrentMove& this_move = std::get<N>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<bool(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getBool<CurrentNHE>();

          // Lazy evaluation
          if (f(this_nhe, st, this_move))
            return true;

          return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAll(st, temp_moves, temp_nhes, c);
        }


        /** Check a predicate on a specific element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if it satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, TupleOfMoves& first_moves, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
            CurrentMove& this_move = std::get<N>(temp_moves).get();
            CurrentMove& this_first_move = std::get<N>(first_moves).get();

            // Instantiate the function with the right template parameters
            std::function<bool(const CurrentNHE&, State&,  CurrentMove&,  CurrentMove&)> f =  c.template getBoolExt<CurrentNHE>();

            return f(this_nhe, st, this_move, this_first_move);
          }
          
          // Otherwise, go down with recursion
          else if (level > 0)
            return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N - 1>::CheckAt(st, temp_moves, temp_nhes, first_moves, c, --level);
#if defined(DEBUG)
          else
            throw std::logic_error("In function CheckAt (recursive case) level is less than zero");
#endif
          // Just to avoid warnings from smart compilers
          return false;
        }

        /** Check a predicate on a specific element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if it satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<N, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<N, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<N>(temp_nhes).get();
            CurrentMove& this_move = std::get<N>(temp_moves).get();

            // Instantiate the function with the right template parameters
            std::function<bool(const CurrentNHE&, State&,  CurrentMove&)> f =  c.template getBool<CurrentNHE>();

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
        /** Run a function on a specific element of @ref Move (and @ref NeighborhoodExplorer) tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, TupleOfMoves& first_moves, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
            CurrentMove& this_move = std::get<0>(temp_moves).get();
            CurrentMove& this_first_move = std::get<0>(first_moves).get();

            // Instantiate the function with the right template parameters
            std::function<void(const CurrentNHE&, State&, CurrentMove&, CurrentMove&)> f =  c.template getVoidExt<CurrentNHE>();
            f(this_nhe, st, this_move, this_first_move);
          }
#if defined(DEBUG)
          else
            throw std::logic_error("In function ExecuteAt (base case) level is not zero");
#endif
        }
        
        /** Run a function on a specific element of @ref Move (and @ref NeighborhoodExplorer) tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static void ExecuteAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
            CurrentMove& this_move = std::get<0>(temp_moves).get();

            // Instantiate the function with the right template parameters
            std::function<void(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getVoid<CurrentNHE>();
            f(this_nhe, st, this_move);
          }
#if defined(DEBUG)
          else
            throw std::logic_error("In function ExecuteAt (base case) level is not zero");
#endif
        }

        /** Run a function on all the elements of the @ref Move (and @ref NeighborhoodExplorer) tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static void ExecuteAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
          CurrentMove& this_move = std::get<0>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<void(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getVoid<CurrentNHE>();
          f(this_nhe, st, this_move);
        }

        /** Run a function on all the elements of the @ref Move (and @ref NeighborhoodExplorer) tuples, then return the result. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static CFtype ComputeAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
            CurrentMove& this_move = std::get<0>(temp_moves).get();

            // Instantiate the function with the right template parameters
            std::function<CFtype(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getCFtype<CurrentNHE>();
            return f(this_nhe, st, this_move);
          }
#if defined(DEBUG)
          else
            throw std::logic_error("In function ComputeAt (base case) level is not zero");
#endif
          // Just to avoid warnings from smart compilers
          return static_cast<CFtype>(0);
        }

        /** Run a function at all the levels of the @ref Move (and @ref NeighborhoodExplorer) tuples, then return the sum of the result. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static CFtype ComputeAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
          CurrentMove& this_move = std::get<0>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<CFtype(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getCFtype<CurrentNHE>();
          return f(this_nhe, st, this_move);
        }

        /** Check a predicate on each element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns the vector of the corresponding boolean results. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static std::vector<bool> Check(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
          CurrentMove& this_move = std::get<0>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<bool(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getBool<CurrentNHE>();
          std::vector<bool> current;
          current.push_back(f(this_nhe, st, this_move));
          return current;
        }

        /** Check a predicate on each element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if all of them satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static bool CheckAll(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
          CurrentMove& this_move = std::get<0>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<bool(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getBool<CurrentNHE>();
          return f(this_nhe, st, this_move);
        }

        /** Check a predicate on each element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if at least one of them satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
        */
        static bool CheckAny(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
          typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
          CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
          CurrentMove& this_move = std::get<0>(temp_moves).get();

          // Instantiate the function with the right template parameters
          std::function<bool(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getBool<CurrentNHE>();
          return f(this_nhe, st, this_move);
        }
        
        /** Check a predicate on a specific element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if it satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, TupleOfMoves& first_moves, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
            CurrentMove& this_move = std::get<0>(temp_moves).get();
            CurrentMove& this_first_move = std::get<0>(first_moves).get();

            // Instantiate the function with the right template parameters
            std::function<bool(const CurrentNHE&, State&, CurrentMove&, CurrentMove&)> f =  c.template getBoolExt<CurrentNHE>();
            return f(this_nhe, st, this_move, this_first_move);
          }
#if defined(DEBUG)
          else
            throw std::logic_error("In function CheckAt (base case) level is not zero");
#endif
          // Just to avoid warnings from smart compilers
          return false;
        }

        /** Check a predicate on a specific element of the @ref Move (and @ref NeighborhoodExplorer) tuples and returns true if it satisfies the predicate. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param temp_nhes tuple of references to associated @ref NeighborhoodExplorer
            @param c @ref Call to execute
            @param level tuple level at which to execute the function (0 is the last element, N - 1 is the first)
        */
        static bool CheckAt(State& st, TupleOfMoves& temp_moves, const TupleOfNHEs& temp_nhes, const Call& c, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get right types (be nice to gcc)
            typedef typename std::tuple_element<0, TupleOfNHEs>::type::type CurrentNHE;
            typedef typename std::tuple_element<0, TupleOfMoves>::type::type CurrentMove;
            CurrentNHE& this_nhe = std::get<0>(temp_nhes).get();
            CurrentMove& this_move = std::get<0>(temp_moves).get();

            // Instantiate the function with the right template parameters
            std::function<bool(const CurrentNHE&, State&, CurrentMove&)> f =  c.template getBool<CurrentNHE>();
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
      static void ExecuteAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& first, const Call& c, int index)
      {
        TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ExecuteAt(st, moves, nhes, first, c, (sizeof...(NHEs) - 1) - index);
      }
      
      /** @copydoc TupleDispatcher::ExecuteAt */
      template <class ...NHEs>
      static void ExecuteAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int index)
      {
        TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ExecuteAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - index);
      }

      /** @copydoc TupleDispatcher::ExecuteAll */
      template <class ...NHEs>
      static void ExecuteAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
      {
        TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ExecuteAll(st, moves, nhes, c);
      }

      /** @copydoc TupleDispatcher::ComputeAt */
      template <class ...NHEs>
      static CFtype ComputeAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int index)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ComputeAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - index);
      }

      /** @copydoc TupleDispatcher::ComputeAll */
      template <class ...NHEs>
      static CFtype ComputeAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::ComputeAll(st, moves, nhes, c);
      }

      /** @copydoc TupleDispatcher::Check */
      template <class ...NHEs>
      static std::vector<bool> Check(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::Check(st, moves, nhes, c);
      }

      /** @copydoc TupleDispatcher::CheckAll */
      template <class ...NHEs>
      static bool CheckAll(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAll(st, moves, nhes, c);
      }

      /** @copydoc TupleDispatcher::CheckAny */
      template <class ...NHEs>
      static bool CheckAny(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAny(st, moves, nhes, c);
      }

      /** @copydoc TupleDispatcher::CheckAt */
      template <class ...NHEs>
      static CFtype CheckAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& first, const Call& c, int index)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAt(st, moves, nhes, first, c, (sizeof...(NHEs) - 1) - index);
      }

      /** @copydoc TupleDispatcher::CheckAt */
      template <class ...NHEs>
      static CFtype CheckAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const std::tuple<std::reference_wrapper<NHEs>...>& nhes, const Call& c, int index)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs) - 1>::CheckAt(st, moves, nhes, c, (sizeof...(NHEs) - 1) - index);
      }

      /** Checks if a @ref Move is active.
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove to check
      */
      template<class N>
      static bool IsActive(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        return m.active;
      }

      /** Generates a random @ref Move in the neighborhood.
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove which must be set
      */
      template<class N>
      static void DoRandomMove(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        n.RandomMove(s, m);
        m.active = true;
      }
      
      /** Generates a random @ref Move in the neighborhood.
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove which must be set
      */
      template<class N>
      static void DoRandomMoveWithFirst(const N& n, State& s, ActiveMove<typename N::MoveType>& m, ActiveMove<typename N::MoveType>& f)
      {
        n.RandomMove(s, m);
        m.active = true;
        f = m;
      }
      

      /** Get the first @ref Move of the neighborhood
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove which must be set
      */
      template<class N>
      static void DoFirstMove(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        n.FirstMove(s, m);
        m.active = true;
      }

      /** Try to produce the next @ref Move of the neighborhood
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove which must be set
      */
      template<class N>
      static bool TryNextMove(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        m.active = n.NextMove(s, m);
        return m.active;
      }
      
      /** Try to produce the next @ref Move of the neighborhood
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove which must be set
      */
      template<class N>
      static bool TryNextMoveWithFirst(const N& n, State& s, ActiveMove<typename N::MoveType>& m, ActiveMove<typename N::MoveType>& f)
      {
        m.active = n.NextMoveWithFirst(s, m, f);
        return m.active;
      }

      /** Executes the @ref Move on the @ref State, modifies it
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the current @ref ActiveMove
      */
      template<class N>
      static void DoMakeMove(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        if (m.active)
        n.MakeMove(s,m);
      }

      /** Sets the active flag of an @ref ActiveMove to false
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove to check
      */
      template<class N>
      static void InitializeInactive(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        m.active = false;
      }

      /** Sets the active flag of an @ref ActiveMove to true
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove to check
      */
      template<class N>
      static void InitializeActive(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        m.active = true;
      }

      /** Computes the cost of making a @ref Move on a state
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove to check
      */
      template<class N>
      static CFtype DoDeltaCostFunction(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        return n.DeltaCostFunction(s, m);
      }

      /** Computes the hard cost of making a @ref Move on a @ref State
          @param n a reference to the @ref Move's @ref NeighborhoodExplorer
          @param s a reference to the current @ref State
          @param m a reference to the  @ref ActiveMove to check
      */
      template<class N>
      static CFtype DoDeltaViolations(const N& n, State& s, ActiveMove<typename N::MoveType>& m)
      {
        return n.DeltaViolations(s, m);
      }
    };

    /** A @ref MultimodalNeighborhoodExplorer that generates the union of @ref NeighborhoodExplorers. */
    template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
    class SetUnionNeighborhoodExplorer : public MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>
    {
    private:

      /** Type of the @ref NeighborhoodExplorer which has a tuple of @ref Move as its @c MoveType. */
      typedef MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...> SuperNeighborhoodExplorer;

      /** Tuple type representing references to @c BaseNeighborhoodExplorers' @ref Move (because we need to set them). */
      typedef typename SuperNeighborhoodExplorer::MoveTypeRefs MoveTypeRefs;

      /** Alias to @ref SuperNeighborhoodExplorer::Call. */
      typedef typename SuperNeighborhoodExplorer::Call Call;

    public:

      /** Inherit constructor from superclass. Not yet supported by all compilers. */
      // using MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::MultimodalNeighborhoodExplorer;

      SetUnionNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
      : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...)
      {
        // If not otherwise specified, initialize the probabilities as 1 / Modality
        this->bias = std::vector<double>(this->Modality(), 1.0 / (double) this->Modality());
      }
      
      SetUnionNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, std::vector<double> bias, BaseNeighborhoodExplorers& ... nhes)
      : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...), bias(bias)
      { }

      /** @copydoc NeighborhoodExplorer::RandomMove */
      virtual void RandomMove(const State& st, typename SuperNeighborhoodExplorer::MoveType& moves) const throw(EmptyNeighborhood)
      {
        // Select random neighborhood explorer with bias (don't assume that they sum up to one)
        double total_bias = 0.0;
        double pick;
        unsigned int selected = 0;

        for (unsigned int i = 0; i < bias.size(); i++)
          total_bias += bias[i];
        pick = Random::Double(0.0, total_bias);

        // Subtract bias until we're on the right neighborhood explorer
        while(pick > this->bias[selected])
        {
          pick -= this->bias[selected];
          selected++;
        }

        Call initialize_inactive(Call::Function::INITIALIZE_INACTIVE);
        Call random_move(SuperNeighborhoodExplorer::Call::Function::RANDOM_MOVE);

        // Convert to references

        MoveTypeRefs r_moves = to_refs(moves);

        // Set all actions to inactive
        SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);

        // Call NeighborhoodExplorer::RandomMove on the selected NeighborhoodExplorer
        SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, random_move, selected);
      }

      /** @copydoc NeighborhoodExplorer::FirstMove */
      virtual void FirstMove(const State& st, typename SuperNeighborhoodExplorer::MoveType& moves) const throw(EmptyNeighborhood)
      {
        // Select first NeighborhoodExplorer
        unsigned int selected = 0;

        Call initialize_inactive(Call::Function::INITIALIZE_INACTIVE);
        Call first_move(Call::Function::FIRST_MOVE);

        // Convert to references
        MoveTypeRefs r_moves = to_refs(moves);

        // Set all actions to inactive
        SuperNeighborhoodExplorer::ExecuteAll(const_cast<State&>(st), r_moves, this->nhes, initialize_inactive);

        // Try picking the first move, if this doesn't work, get to the next NeighborhoodExplorer (and so on)
        while (selected < this->Modality())
        {
          try
          {
            SuperNeighborhoodExplorer::ExecuteAt(const_cast<State&>(st), r_moves, this->nhes, first_move, selected);
            break;
          }
          catch (EmptyNeighborhood e)
          { }
          selected++;
        }

        // If even the last NeighborhoodExplorer has an EmptyNeighborhood, throw an EmptyNeighborhood for the SetUnionNeighborhoodExplorer
        if (selected == this->Modality())
        throw EmptyNeighborhood();
      }

      /** @copydoc NeighborhoodExplorer::NextMove */
      virtual bool NextMove(const State& st, typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
        // Select the current active NeighborhoodExplorer
        unsigned int selected = this->CurrentActiveMove(st, moves);

        Call try_next_move(Call::Function::TRY_NEXT_MOVE);

        // Convert to references
        MoveTypeRefs r_moves = to_refs(moves);

        // If the current NeighborhoodExplorer has a next move, return true
        if (SuperNeighborhoodExplorer::CheckAt(const_cast<State&>(st), r_moves, this->nhes, try_next_move, selected))
        return true;

        // Otherwise, get to the next one
        do
        {
          selected++;
          if (selected == this->Modality())
          return false;
          try
          {
            // Call FirstMove on the current NeighborhoodExplorer
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
      virtual CFtype DeltaCostFunction(const State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
        // Select the current active NeighborhoodExplorer
        unsigned int selected = this->CurrentActiveMove(st, moves);

        // Compute delta cost
        Call delta_cost_function(Call::Function::DELTA_COST_FUNCTION);

        // Convert to references to non-const
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        // Return cost
        return SuperNeighborhoodExplorer::ComputeAt(const_cast<State&>(st), r_moves, this->nhes, delta_cost_function, selected);
      }

      /** @copydoc NeighborhoodExplorer::DeltaViolations */
      virtual CFtype DeltaViolations(const State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
        // Select the current active NeighborhoodExplorer
        unsigned int selected = this->CurrentActiveMove(st, moves);

        // Compute delta cost
        Call delta_violations(Call::Function::DELTA_VIOLATIONS);

        // Convert to references to non-const
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        // Return cost
        return SuperNeighborhoodExplorer::ComputeAt(const_cast<State&>(st), r_moves, this->nhes, delta_violations, selected);
      }

      /** @copydoc NeighborhoodExplorer::MakeMove */
      virtual void MakeMove(State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
        // Select current active move
        unsigned int selected = this->CurrentActiveMove(st, moves);
        Call make_move(Call::Function::MAKE_MOVE);

        // Convert to references to non-const
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        // Execute move on state
        SuperNeighborhoodExplorer::ExecuteAt(st, r_moves, this->nhes, make_move, selected);
      }

    protected:

      std::vector<double> bias;

      /** Computes the index of the current @ref ActiveMove.
          @param st current @ref State
          @param moves tuple of @ref Move
      */
      int CurrentActiveMove(const State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
        Call is_active(Call::Function::IS_ACTIVE);
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        std::vector<bool> moves_status = SuperNeighborhoodExplorer::Check(const_cast<State&>(st), r_moves, this->nhes, is_active);

        return (int)std::distance(moves_status.begin(), std::find_if(moves_status.begin(), moves_status.end(), [](bool element) { return element; }));
      }
    };

    /** A @ref MultimodalNeighborhoodExplorer that generates the Cartesian product of @ref NeighborhoodExplorer. */
    template <class Input, class State, typename CFtype, class ... BaseNeighborhoodExplorers>
    class CartesianProductNeighborhoodExplorer : public MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>
    {
    private:
      
      /** Type of the @ref NeighborhoodExplorer which has a tuple of @ref Move as its move type. */
      typedef MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...> SuperNeighborhoodExplorer;

      /** Tuple type representing references to @c BaseNeighborhoodExplorers' @ref Move (because we need to set them). */
      typedef typename SuperNeighborhoodExplorer::MoveTypeRefs MoveTypeRefs;

      /** Alias to @ref SuperNeighborhoodExplorer::Call. */
      typedef typename SuperNeighborhoodExplorer::Call Call;

      /** Type of this @ref MultimodalNeighborhoodExplorer. */
      typedef CartesianProductNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers...> ThisNeighborhoodExplorer;

      /** Tuple dispatcher specialization, to handle comparisons between moves. */
      template <class TupleOfMoves, class TupleOfNHEs, std::size_t N>
      struct TupleDispatcher : public SuperNeighborhoodExplorer::template TupleDispatcher<TupleOfMoves, TupleOfNHEs, N>
      {
        /** Compare (n)th and (n+1)th @ref Move according to a predicate. We don't have base case because we don't want to get down to zero.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param cp_nhe reference to this @MultimodalNeighborhoodExplorer
            @param level tuple level(s) at which to compare the @ref Move (0 is the last element, N - 1 is the first)
        */
        static bool CompareMovesAt(State& st, TupleOfMoves& temp_moves, ThisNeighborhoodExplorer& cp_nhe, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            // Get last element of the tuples
            auto this_move = std::get<N-1>(temp_moves).get().RawMove();
            auto next_move = std::get<N>(temp_moves).get().RawMove();

            // Call on the last element
            return IsRelated(this_move, next_move);
          }
          // Otherwise go down with recursion
          else if (level > 0)
            return TupleDispatcher<TupleOfMoves, TupleOfNHEs, N-1>::CompareMovesAt(st, temp_moves, cp_nhe, --level);

#if defined(DEBUG)
          else
            throw std::logic_error("In function CompareMovesAt (recursive case) level is less than zero");
#endif
          // Just to accomodate smart compilers
          return false;
        }

        /** Compare (n)th and (n+1)th moves according to a predicate. We don't have base case because we don't want to get down to zero.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param cp_nhe reference to this @MultimodalNeighborhoodExplorer
        */
        static std::vector<bool> CompareMoves(State& st, TupleOfMoves& temp_moves, ThisNeighborhoodExplorer& cp_nhe)
        {
          std::vector<bool> current, others;
          others = TupleDispatcher<TupleOfMoves, TupleOfNHEs, N-1>::CompareMoves(st, temp_moves, cp_nhe);

          // Get last element of the tuples
          auto this_move = std::get<N-1>(temp_moves).get().RawMove();
          auto next_move = std::get<N>(temp_moves).get().RawMove();

          // Call on the last element
          current.push_back(IsRelated(this_move, next_move));
          current.insert(current.begin(), others.begin(), others.end());

          return current;
        }
      };

      /** Base case of the recursive TupleDispatcher. */
      template <class TupleOfMoves, class TupleOfNHEs>
      struct TupleDispatcher<TupleOfMoves, TupleOfNHEs, 0>
      {
        /** Run a function on a specific element of the tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param cp_nhe reference to this @MultimodalNeighborhoodExplorer
            @param level tuple level(s) at which to compare the @ref Move (0 is the last element, N - 1 is the first)
        */
        static bool CompareMovesAt(State& st, TupleOfMoves& temp_moves, const ThisNeighborhoodExplorer& cp_nhe, int level)
        {
          // If we're at the right level of the recursion
          if (level == 0)
          {
            return true;
          }
#if defined(DEBUG)
          else
            throw std::logic_error("In function CompareMovesAt (base case) level is not zero");
#endif
          // just to prevent warnings from smart compilers
          return false;
        }

        /** Run a function on a specific element of the tuples. Based on compile-time recursion.
            @param st reference to the current @ref State
            @param temp_moves tuple of references to @ref ActiveMove
            @param cp_nhe reference to this @MultimodalNeighborhoodExplorer
        */
        static std::vector<bool> CompareMoves(State& st, TupleOfMoves& temp_moves, const ThisNeighborhoodExplorer& cp_nhe)
        {
          return std::vector<bool>(0);
        }
      };


      /** @copydoc TupleDispatcher::CompareMovesAt */
      template <class ...NHEs>
      static bool CompareMovesAt(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const ThisNeighborhoodExplorer& cp_nhe, int index)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs)-1>::CompareMovesAt(st, moves, const_cast<ThisNeighborhoodExplorer&>(cp_nhe), (sizeof...(NHEs) - 1) - index);
      }

      /** @copydoc TupleDispatcher::CompareMoves */
      template <class ...NHEs>
      static std::vector<bool> CompareMoves(State& st, std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>& moves, const ThisNeighborhoodExplorer& cp_nhe)
      {
        return TupleDispatcher<std::tuple<std::reference_wrapper<ActiveMove<typename NHEs::MoveType>>...>, std::tuple<std::reference_wrapper<NHEs>...>, sizeof...(NHEs)-1>::CompareMoves(st, moves, const_cast<ThisNeighborhoodExplorer&>(cp_nhe));
      }

#if defined(DEBUG)
      /** Check that all the @ref ActiveMove, inside a tuple of @ref Move, are active. */
      void VerifyAllActives(const State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const throw (std::logic_error)
      {
        Call is_active(SuperNeighborhoodExplorer::Call::Function::IS_ACTIVE);
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        std::vector<bool> active = SuperNeighborhoodExplorer::Check(const_cast<State&>(st), r_moves, this->nhes, is_active);
        for (bool v : active)
          if (!v)
            throw std::logic_error("Some of the moves were not active in a composite CartesianProduct neighborhood explorer");
      }

      /** Check that all the @ref Move, inside a tuple of @ref Move, are related. */
      void VerifyAllRelated(const State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const throw (std::logic_error)
      {
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        std::vector<bool> are_related = CompareMoves<BaseNeighborhoodExplorers...>(const_cast<State&>(st), r_moves, *this);
        for (bool v : are_related)
          if (!v)
            throw std::logic_error("Some of the moves were not related in a composite CartesianProduct neighborhood explorer");
      }
#endif

    public:

      /** Tuple type representing references to BaseNeighborhoodExplorers. */
      typedef std::tuple<std::reference_wrapper<BaseNeighborhoodExplorers> ...> NeighborhoodExplorerTypes;

      /** Inherit constructor from superclass. Not yet supported by all compilers. */
      // using MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>::MultimodalNeighborhoodExplorer;

      CartesianProductNeighborhoodExplorer(Input& in, StateManager<Input,State,CFtype>& sm, std::string name, BaseNeighborhoodExplorers& ... nhes)
      : MultimodalNeighborhoodExplorer<Input, State, CFtype, BaseNeighborhoodExplorers ...>(in, sm, name, nhes ...)
      { }

      /** @copydoc NeighborhoodExplorer::RandomMove */
      virtual void RandomMove(const State& st, typename SuperNeighborhoodExplorer::MoveType& moves) const throw(EmptyNeighborhood)
      {
        
        Call random_move(Call::Function::RANDOM_MOVE_WITH_FIRST);
        Call make_move(Call::Function::MAKE_MOVE);
        Call try_next_move_with_first(Call::Function::TRY_NEXT_MOVE_WITH_FIRST);

        // Convert to references
        MoveTypeRefs r_moves = to_refs(moves);

        // first moves generated on each neighborhood
        typename SuperNeighborhoodExplorer::MoveType first_moves;
        MoveTypeRefs f_moves = to_refs(first_moves);
        

        // The chain of states generated during the execution of the multimodal move (including the first one, but excluding the last)
        std::vector<State> temp_states(this->Modality(), State(this->in));

        temp_states[0] = st;
        temp_states[1] = temp_states[0];

        // Generate first move starting from the initial state
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[0], r_moves, this->nhes, f_moves, random_move, 0);
      
        // If modality is 1 we're finished
        if (this->Modality() == 1)
          return;

        // Execute first move and save new state in next state
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[1], r_moves, this->nhes, make_move, 0);

        int i = 1;
        while (i < static_cast<int>(this->Modality()))
        {
          try
          {
            // Generate random move
            SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, f_moves, random_move, i);

            while (!CompareMovesAt<BaseNeighborhoodExplorers...>(temp_states[i], r_moves, *this, i))
            {
              if (!SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, f_moves, try_next_move_with_first, i))
                throw EmptyNeighborhood(); // just to enter backtracking
            }

            if (i == static_cast<int>(this->Modality()) - 1)
            {
              // The whole chain of moves has been dispatched
#if defined(DEBUG)
              VerifyAllActives(st, moves);
              VerifyAllRelated(st, moves);
#endif
              return;
            }
            
            // Duplicate current state
            temp_states[i+1] = temp_states[i];

            // Make move (otherwise we don't have next state for generating the next move)
            SuperNeighborhoodExplorer::ExecuteAt(temp_states[i+1], r_moves, this->nhes, make_move, i);
          }
          catch (EmptyNeighborhood e)
          {
            while (i > 0)
            {
              // Backtrack
              i--;

              // Reset state which was modified during visit
              temp_states[i+1] = temp_states[i];

              // Generate first move
              //SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, first_move, i);

              bool empty = false;

              do
              {
                // We can't throw an exception to backtrack since we're already backtracking
                if (!SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, f_moves, try_next_move_with_first, i))
                  empty = true;
              }
              while (!CompareMovesAt<BaseNeighborhoodExplorers...>(temp_states[i], r_moves, *this, i));

              // a move to be performed has been found
              if (!empty)
              {
                // execute generated move
                SuperNeighborhoodExplorer::ExecuteAt(temp_states[i+1], r_moves, this->nhes, make_move, i);
                break;
              }
              else
              if (i == 0)
              {
                // No combination whatsoever could be extended as a "first move" on the first neighborhood
                throw EmptyNeighborhood();
              }
            }
          }
          i++;
        }
        
#if defined(DEBUG)
        VerifyAllActives(st, moves);
#endif

      }

      /** @copydoc NeighborhoodExplorer::FirstMove */
      virtual void FirstMove(const State& st, typename SuperNeighborhoodExplorer::MoveType& moves) const throw(EmptyNeighborhood)
      {
        Call first_move(Call::Function::FIRST_MOVE);
        Call make_move(Call::Function::MAKE_MOVE);
        Call try_next_move(Call::Function::TRY_NEXT_MOVE);

        // Convert to references
        MoveTypeRefs r_moves = to_refs(moves);

        // The chain of states generated during the execution of the multimodal move (including the first one, but excluding the last)
        std::vector<State> temp_states(this->Modality(), State(this->in));

        temp_states[0] = st;
        temp_states[1] = temp_states[0];

        // Generate first move starting from the initial state
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[0], r_moves, this->nhes, first_move, 0);

        // If modality is 1 we're finished
        if (this->Modality() == 1)
        return;

        // Execute first move and save new state in next state
        SuperNeighborhoodExplorer::ExecuteAt(temp_states[1], r_moves, this->nhes, make_move, 0);

        int i = 1;
        while (i < static_cast<int>(this->Modality()))
        {
          try
          {
            // Generate first move
            SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, first_move, i);

            while (!CompareMovesAt<BaseNeighborhoodExplorers...>(temp_states[i], r_moves, *this, i))
            {
              if (!SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, try_next_move, i))
              throw EmptyNeighborhood(); // just to enter backtracking
            }

            if (i == static_cast<int>(this->Modality()) - 1)
            {
              // The whole chain of moves has been dispatched
              #if defined(DEBUG)
              VerifyAllActives(st, moves);
              VerifyAllRelated(st, moves);
              #endif
              return;
            }
            // Duplicate current state
            temp_states[i+1] = temp_states[i];

            // Make move (otherwise we don't have next state for generating the next move)
            SuperNeighborhoodExplorer::ExecuteAt(temp_states[i+1], r_moves, this->nhes, make_move, i);
          }
          catch (EmptyNeighborhood e)
          {
            while (i > 0)
            {
              // Backtrack
              i--;

              // Reset state which was modified during visit
              temp_states[i+1] = temp_states[i];

              // Generate first move
              //SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, first_move, i);

              bool empty = false;

              do
              {
                // We can't throw an exception to backtrack since we're already backtracking
                if (!SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, try_next_move, i))
                empty = true;
              }
              while (!CompareMovesAt<BaseNeighborhoodExplorers...>(temp_states[i], r_moves, *this, i));

              // a move to be performed has been found
              if (!empty)
              {
                // execute generated move
                SuperNeighborhoodExplorer::ExecuteAt(temp_states[i+1], r_moves, this->nhes, make_move, i);
                break;
              }
              else
              if (i == 0)
              {
                // No combination whatsoever could be extended as a "first move" on the first neighborhood
                throw EmptyNeighborhood();
              }
            }
          }
          i++;
        }
      }

      /** @copydoc NeighborhoodExplorer::NextMove */
      virtual bool NextMove(const State& st, typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
        Call first_move(Call::Function::FIRST_MOVE);
        Call make_move(Call::Function::MAKE_MOVE);
        Call try_next_move(Call::Function::TRY_NEXT_MOVE);

        int i = 0;

        // Convert to references
        MoveTypeRefs r_moves = to_refs(moves);

        // The chain of states generated during the execution of the multimodal move (including the first one, but excluding the last)
        std::vector<State> temp_states(this->Modality(), State(this->in));

        temp_states[0] = st;

        // create and initialize all the remaining states in the chain
        for (unsigned int i = 1; i < this->Modality(); i++)
        {
          temp_states[i] = temp_states[i - 1];
          SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, i - 1);
        }

        // attempt to find a next move in the last component of the move tuple
        i = this->Modality() - 1;
        while (SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, try_next_move, i))
        if (CompareMovesAt<BaseNeighborhoodExplorers...>(temp_states[i], r_moves, *this, i))
        return true;

        bool backtracking = true;
        while (i < static_cast<int>(this->Modality()))
        {
          // backtracking to the first available component that has a next move
          while (backtracking && i > 0)
          {
            i--; // backtrack
            temp_states[i+1] = temp_states[i];

            while (SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, try_next_move, i))
            {
              if (CompareMovesAt<BaseNeighborhoodExplorers...>(temp_states[i], r_moves, *this, i))
              {
                SuperNeighborhoodExplorer::ExecuteAt(temp_states[i + 1], r_moves, this->nhes, make_move, i);
                i++;
                backtracking = false;
                break;
              }
            }
          }

          if (i == 0)
          return false;
          backtracking = false;
          // forward trying a set of first moves
          try
          {

            SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, first_move, i);

            while (!CompareMovesAt<BaseNeighborhoodExplorers...>(temp_states[i], r_moves, *this, i))
            {
              if (!SuperNeighborhoodExplorer::CheckAt(temp_states[i], r_moves, this->nhes, try_next_move, i))
              throw EmptyNeighborhood(); // just to enter backtracking
            }

            if (i == static_cast<int>(this->Modality()) - 1)
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
      virtual CFtype DeltaCostFunction(const State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
#if defined(DEBUG)
        VerifyAllActives(st, moves);
        VerifyAllRelated(st, moves);
#endif

        Call do_delta_cost_function(Call::Function::DELTA_COST_FUNCTION);
        Call make_move(Call::Function::MAKE_MOVE);

        CFtype sum = static_cast<CFtype>(0);
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        std::vector<State> temp_states(this->Modality(), State(this->in)); // the chain of states
        temp_states[0] = st; // the first state is a copy of to st
        // create and initialize all the remaining states in the chain
        sum = SuperNeighborhoodExplorer::ComputeAt(temp_states[0], r_moves, this->nhes, do_delta_cost_function, 0);
        for (unsigned int i = 1; i < this->Modality(); i++)
        {
          temp_states[i] = temp_states[i - 1];
          SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, i - 1);
          sum += SuperNeighborhoodExplorer::ComputeAt(temp_states[i], r_moves, this->nhes, do_delta_cost_function, i);
        }

        return sum;
      }

      /** @copydoc NeighborhoodExplorer::DeltaViolations */
      virtual CFtype DeltaViolations(const State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
#if defined(DEBUG)
        VerifyAllActives(st, moves);
        VerifyAllRelated(st, moves);
#endif

        Call do_delta_violations(Call::Function::DELTA_VIOLATIONS);
        Call make_move(Call::Function::MAKE_MOVE);

        CFtype sum = static_cast<CFtype>(0);
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        std::vector<State> temp_states(this->Modality(), State(this->in)); // the chain of states
        temp_states[0] = st; // the first state is a copy of to st
        // create and initialize all the remaining states in the chain
        sum = SuperNeighborhoodExplorer::ComputeAt(temp_states[0], r_moves, this->nhes, do_delta_violations, 0);
        for (unsigned int i = 1; i < this->Modality(); i++)
        {
          temp_states[i] = temp_states[i - 1];
          SuperNeighborhoodExplorer::ExecuteAt(temp_states[i], r_moves, this->nhes, make_move, i - 1);
          sum += SuperNeighborhoodExplorer::ComputeAt(temp_states[i], r_moves, this->nhes, do_delta_violations, i);
        }

        return sum;
      }

      /** @copydoc NeighborhoodExplorer::MakeMove */
      virtual void MakeMove(State& st, const typename SuperNeighborhoodExplorer::MoveType& moves) const
      {
#if defined(DEBUG)
        VerifyAllActives(st, moves);
        VerifyAllRelated(st, moves);
#endif
        Call make_move(Call::Function::MAKE_MOVE);
        auto c_moves = const_cast<decltype(moves)>(moves);
        MoveTypeRefs r_moves = to_refs(c_moves);
        SuperNeighborhoodExplorer::ExecuteAll(st, r_moves, this->nhes, make_move);
      }
    };
  }
}

#endif // _MULTIMODALNEIGHBORHOOD_EXPLORER_HH_
