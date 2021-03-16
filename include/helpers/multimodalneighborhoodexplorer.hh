#pragma once

#include "helpers/neighborhoodexplorer.hh"
#include <functional>
#include "utils/FastFunc.hh"
#include "utils/tuple.hh"

#include <any>
#include <typeindex>
#include <unordered_map>
#include <functional>

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** Template class to incapsulate a boolean flag that marks a @ref Move active or inactive in the context of a @ref MultimodalNeighborhoodExplorer. */
    template <class Move>
    class ActiveMove : public Move
    {
    public:
      bool active;
      
      /** Get the raw @ref Move inside this object. */
      Move &RawMove()
      {
        return *this;
      }
      
      const Move& RawMove() const
      {
        return *this;
      }
    };
    
    /** Input operator for @ref ActiveMove, just forwards to input operator for @ref Move.
     @param is input stream, by default @c std::cin
     @param m @ref Move to read
     */
    template <typename Move>
    std::istream &operator>>(std::istream &is, ActiveMove<Move> &m)
    {
      is >> static_cast<Move &>(m);
      return is;
    }
    
    /** Output operator for @ref ActiveMove, just forwards to output operator for @ref Move.
     @param os output stream, by default @c std::cout
     @param m @ref Move to print
     */
    template <typename Move>
    std::ostream &operator<<(std::ostream &os, const ActiveMove<Move> &m)
    {
      if (m.active)
        os << static_cast<const Move &>(m);
      return os;
    }
    
    /** Equality operator for @ref ActiveMove, forwards to equality operator for @ref Move and checks for same @c active flag.
     @param mv1 first @ref Move
     @param mv2 second @ref Move
     @return true if the two raw @ref Move are the same and the @active flag is the equal
     */
    template <class Move>
    bool operator==(const ActiveMove<Move> &mv1, const ActiveMove<Move> &mv2)
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
    bool operator<(const ActiveMove<Move> &mv1, const ActiveMove<Move> &mv2)
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
      
    /* This namespace will hide implementation components */
    namespace Impl
    {
      
      /** Creates a callable fast-function with a given signature */
#ifndef MSVC
      template <typename T, typename return_type, typename... params>
      FastFunc<return_type(params...)>
      makeFastFunc(T *obj, return_type (T::*f)(params...) const)
      {
        return FastFunc<return_type(params...)>(obj, f);
      }
#else
      template <typename T, typename return_type, typename... params>
      std::function<return_type(params...)>
      makeFunction2(T *obj, return_type (T::*f)(params...) const)
      {
        auto mf = std::mem_fn<return_type(params...) const>(f);
        return std::bind(mf, obj, std::placeholders::_1, std::placeholders::_2);
      }
      template <typename T, typename return_type, typename... params>
      std::function<return_type(params...)>
      makeFunction3(T *obj, return_type (T::*f)(params...) const)
      {
        auto mf = std::mem_fn<return_type(params...) const>(f);
        return std::bind(mf, obj, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
      }
#endif
    
      template <typename T, std::size_t...Is>
      constexpr std::array<T, sizeof...(Is)> make_array(const T& value, std::index_sequence<Is...>)
      {
        return {{(static_cast<void>(Is), value)...}};
      }
      
      /** Helper class for dispatching tuples and applying functions to tuple elements. General recursion case.
       * This version will handle procedures (i.e., functions returning void). */
      template <class Solution, class FuncsTuple, class MovesTuple, size_t N>
      struct VTupleDispatcher
      {
        // First Move
        static void execute_at(long level, const Solution &st, const FuncsTuple &funcs, MovesTuple &moves)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            auto &this_move = std::get<0>(moves).get();
            f(st, this_move);
          }
          else
          {
            auto moves_tail = tuple_tail(moves);
            const auto funcs_tail = tuple_tail(funcs);
            VTupleDispatcher<Solution, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail);
          }
        }
        // Make Move
        static void execute_at(long level, Solution& st, const FuncsTuple &funcs, const MovesTuple &moves)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            const auto &this_move = std::get<0>(moves).get();
            f(st, this_move);
          }
          else
          {
            const auto moves_tail = tuple_tail(moves);
            const auto funcs_tail = tuple_tail(funcs);
            VTupleDispatcher<Solution, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail);
          }
        }
      };
      
      /** Helper class for dispatching tuples and applying functions to tuple elements. Base recursion case.
       * This version will handle procedures (i.e., functions returning void). */
      template <class Solution, class FuncsTuple, class MovesTuple>
      struct VTupleDispatcher<Solution, FuncsTuple, MovesTuple, 0>
      {
        // First Move
        static void execute_at(long level, const Solution &st, const FuncsTuple &funcs, MovesTuple &moves)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            auto &this_move = std::get<0>(moves).get();
            f(st, this_move);
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
        // Make Move
        static void execute_at(long level, Solution&st, const FuncsTuple &funcs, const MovesTuple &moves)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            const auto this_move = std::get<0>(moves);
            f(st, this_move);
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
      };
      
      /** Helper class for dispatching tuples and applying functions to tuple elements. General recursion case.
       * This version will handle functions. */
      template <class ReturnType, class Solution, class FuncsTuple, class MovesTuple, size_t N>
      struct TupleDispatcher
      {
        // Next Move (returns bool)
        static ReturnType execute_at(long level, const Solution &st, const FuncsTuple &funcs, MovesTuple &moves)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            auto &this_move = std::get<0>(moves).get();
            return f(st, this_move);
          }
          else
          {
            auto moves_tail = tuple_tail(moves);
            const auto funcs_tail = tuple_tail(funcs);
            return TupleDispatcher<ReturnType, Solution, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail);
          }
        }
        // DeltaCostFunction (returns a composite value)
        static ReturnType execute_at(long level, const Solution &st, const FuncsTuple &funcs, const MovesTuple &moves, const std::vector<double> &weights)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            const auto &this_move = std::get<0>(moves).get();
            return f(st, this_move, weights);
          }
          else
          {
            const auto moves_tail = tuple_tail(moves);
            const auto funcs_tail = tuple_tail(funcs);
            return TupleDispatcher<ReturnType, Solution, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail, weights);
          }
        }
        static ReturnType execute_at(long level, const Solution &st, const FuncsTuple &funcs)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            return f(st);
          }
          else
          {
            MovesTuple mt;
            const auto moves_tail = tuple_tail(mt);
            const auto funcs_tail = tuple_tail(funcs);
            return TupleDispatcher<ReturnType, Solution, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail);
          }
        }
      };
      /** Helper class for dispatching tuples and applying functions to tuple elements. Base recursion case.
       * This version will handle functions. */
      template <class ReturnType, class Solution, class FuncsTuple, class MovesTuple>
      struct TupleDispatcher<ReturnType, Solution, FuncsTuple, MovesTuple, 0>
      {
        // Next Move (returns bool)
        static ReturnType execute_at(long level, const Solution &st, const FuncsTuple &funcs, MovesTuple &moves)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            auto &this_move = std::get<0>(moves).get();
            return f(st, this_move);
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
        // DeltaCostFunction (returns a composite value)
        static ReturnType execute_at(long level, const Solution &st, const FuncsTuple &funcs, const MovesTuple &moves, const std::vector<double> &weights)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            const auto &this_move = std::get<0>(moves).get();
            return f(st, this_move, weights);
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
        static ReturnType execute_at(long level, const Solution &st, const FuncsTuple &funcs)
        {
          if (level == 0)
          {
            const auto &f = std::get<0>(funcs);
            return f(st);
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
      };
      
      /** Helper class for dispatching move tuples. General recursion case.
       * This version will handle functions. */
      template <class MovesTuple, size_t N>
      struct MoveDispatcher
      {
        static void set_all_activity(MovesTuple &moves, bool value)
        {
          auto &this_move = std::get<0>(moves).get();
          this_move.active = value;
          auto moves_tail = tuple_tail(moves);
          MoveDispatcher<decltype(moves_tail), N - 1>::set_all_activity(moves_tail, value);
        }
        static void set_activity_at(long level, MovesTuple &moves, bool value)
        {
          if (level == 0)
          {
            auto &this_move = std::get<0>(moves).get();
            this_move.active = value;
          }
          else
          {
            auto moves_tail = tuple_tail(moves);
            MoveDispatcher<decltype(moves_tail), N - 1>::set_activity_at(--level, moves_tail, value);
          }
        }
        static void copy_move_at(long level, MovesTuple &target, const MovesTuple &source)
        {
          if (level == 0)
          {
            auto &this_target = std::get<0>(target).get();
            const auto &this_source = std::get<0>(target).get();
            this_target = this_source;
          }
          else
          {
            auto target_tail = tuple_tail(target);
            auto source_tail = tuple_tail(source);
            MoveDispatcher<decltype(target_tail), N - 1>::copy_move_at(--level, target_tail, source_tail);
          }
        }
        static bool equal_at(long level, MovesTuple moves_1, MovesTuple moves_2)
        {
          if (level == 0)
          {
            const auto &this_move_1 = std::get<0>(moves_1).get();
            const auto &this_move_2 = std::get<0>(moves_2).get();
            return this_move_1 == this_move_2;
          }
          else
          {
            const auto& moves_1_tail = tuple_tail(moves_1);
            const auto& moves_2_tail = tuple_tail(moves_2);
            return MoveDispatcher<decltype(moves_1_tail), N - 1>::equal_at(--level, moves_1_tail, moves_2_tail);
          }
        }
        static bool are_related(long level, const MovesTuple &moves, const std::unordered_map<std::type_index, std::any> &related_funcs)
        {
          if (level == 0)
          {
            const auto &this_move = std::get<0>(moves).get();
            const auto &next_move = std::get<1>(moves).get();
            auto it = related_funcs.find(std::type_index(typeid(std::function<bool(const decltype(this_move) &, const decltype(next_move) &)>)));
            if (it == related_funcs.end())
              return true;
            else
              return std::any_cast<std::function<bool(const decltype(this_move) &, const decltype(next_move) &)>>(it->second)(this_move, next_move);
          }
          else
          {
            auto moves_tail = tuple_tail(moves);
            return MoveDispatcher<decltype(moves_tail), N - 1>::are_related(--level, moves_tail, related_funcs);
          }
        }
        static size_t get_first_active(const MovesTuple &moves, long level)
        {
          const auto &this_move = std::get<0>(moves).get();
          if (this_move.active)
            return level;
          else
          {
            auto moves_tail = tuple_tail(moves);
            return MoveDispatcher<decltype(moves_tail), N - 1>::get_first_active(moves_tail, ++level);
          }
        }
      };
                      
      /** Helper class for dispatching move tuples. Base recursion case.
       * This version will handle functions. */
      template <class MovesTuple>
      struct MoveDispatcher<MovesTuple, 0>
      {
        static void set_all_activity(MovesTuple &moves, bool value)
        {
          auto &this_move = std::get<0>(moves).get();
          this_move.active = value;
        }
        static void set_activity_at(long level, MovesTuple &moves, bool value)
        {
          if (level == 0)
          {
            auto &this_move = std::get<0>(moves).get();
            this_move.active = value;
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
        static void copy_move_at(long level, MovesTuple &target, const MovesTuple &source)
        {
          if (level == 0)
          {
            auto &this_target = std::get<0>(target).get();
            const auto &this_source = std::get<0>(target).get();
            this_target = this_source;
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
        static bool equal_at(long level, MovesTuple moves_1, MovesTuple moves_2)
        {
          if (level == 0)
          {
            const auto &this_move_1 = std::get<0>(moves_1).get();
            const auto &this_move_2 = std::get<0>(moves_2).get();
            return this_move_1 == this_move_2;
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
        static bool are_related(long level, const MovesTuple &moves, const std::unordered_map<std::type_index, std::any> &related_funcs)
        {
          throw std::logic_error("End of tuple recursion");
        }
        static bool are_related(long level, const MovesTuple &moves_1, const MovesTuple &moves_2, const std::unordered_map<std::type_index, std::any> &inverse_funcs)
        {
          if (level == 0)
          {
            const auto& this_move_1  = std::get<0>(moves_1).get();
            const auto& this_move_2 = std::get<0>(moves_2).get();
            auto it = inverse_funcs.find(std::type_index(typeid(std::function<bool(const decltype(this_move_1) &, const decltype(this_move_1) &)>)));
            if (it == inverse_funcs.end())
              // the default definition is that each move is inverse of itself
              return this_move_1 == this_move_2;
            else
              return std::any_cast<std::function<bool(const decltype(this_move_1) &, const decltype(this_move_1) &)>>(it->second)(this_move_1, this_move_2);
            return this_move_1 == this_move_2;
          }
          else
            throw std::logic_error("End of tuple recursion");
        }
        static size_t get_first_active(const MovesTuple &moves, long level)
        {
          const auto &this_move = std::get<0>(moves).get();
          if (this_move.active)
            return level;
          else
            throw std::logic_error("End of tuple recursion");
        }
      };
    
      /** Helper class for dispatching tuples and applying functions to tuple elements. General recursion case.
       * This version will apply a the function to a solution returning the array of transformed values. */
      template <class Solution, class FuncsTuple, class MovesTuple, size_t N>
      struct TupleApplier
      {
        static vector<Solution> apply_all(const Solution& st, const FuncsTuple &funcs, const MovesTuple &moves)
        {
          const auto &f = std::get<0>(funcs);
          const auto &this_move = std::get<0>(moves).get();
          Solution sol = st;
          f(sol, this_move);
          
          const auto moves_tail = tuple_tail(moves);
          const auto funcs_tail = tuple_tail(funcs);
          std::vector<Solution> results = TupleApplier<Solution, decltype(funcs_tail), decltype(moves_tail), N - 1>::apply_all(sol, funcs_tail, moves_tail);
          results.push_back(sol);
          return results;
        }
        
        static void apply_chain(Solution& st, const FuncsTuple &funcs, const MovesTuple &moves)
        {
          const auto &f = std::get<0>(funcs);
          const auto &this_move = std::get<0>(moves).get();
          f(st, this_move);
          
          const auto moves_tail = tuple_tail(moves);
          const auto funcs_tail = tuple_tail(funcs);
          TupleApplier<Solution, decltype(funcs_tail), decltype(moves_tail), N - 1>::apply_all(st, funcs_tail, moves_tail);
        }
      };
    
    template <class Solution, class FuncsTuple, class MovesTuple>
    struct TupleApplier<Solution, FuncsTuple, MovesTuple, 0>
    {
      static vector<Solution> apply_all(const Solution& st, const FuncsTuple &funcs, const MovesTuple &moves)
      {
        const auto &f = std::get<0>(funcs);
        const auto &this_move = std::get<0>(moves).get();
        std::vector<Solution> sol(1, st);
        f(sol[0], this_move);
        
        return sol;
      }
      
      static void apply_chain(Solution& st, const FuncsTuple &funcs, const MovesTuple &moves)
      {
        const auto &f = std::get<0>(funcs);
        const auto &this_move = std::get<0>(moves).get();
        f(st, this_move);
      }
    };
    
    /** Helper class for dispatching move tuples. General recursion case.
     * This version will handle functions. */
    template <class MovesTuple1, class MovesTuple2, size_t N1, size_t N2>
    struct BiMoveDispatcher
    {
      static bool are_inverse(long level_1, long level_2, const MovesTuple1 &moves_1, const MovesTuple2 &moves_2, const std::unordered_map<std::type_index, std::any> &inverse_funcs)
      {
        if (level_1 == 0 && level_2 == 0)
        {
          const auto &this_move_1 = std::get<0>(moves_1).get();
          const auto &this_move_2 = std::get<0>(moves_2).get();
          // searches whether there is an inverse for the combination of this_move_1 and this_move_2 in the registry
          auto it = inverse_funcs.find(std::type_index(typeid(std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>)));
          if (it != inverse_funcs.end()) // the inverse for the combination this_move_1 and this_move_2 is present
            // casts back the inverse function from the std::any type to the right one
            return std::any_cast<std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>>(it->second)(this_move_1, this_move_2);
          else
            return false;
        }
        else if (level_1 == 0)
        {
          auto moves_tail_2 = tuple_tail(moves_2);
          return BiMoveDispatcher<decltype(moves_1), decltype(moves_tail_2), N1, N2 - 1>::are_inverse(0, --level_2, moves_1, moves_tail_2, inverse_funcs);
        }
        else if (level_2 == 0)
        {
          auto moves_tail_1 = tuple_tail(moves_1);
          return BiMoveDispatcher<decltype(moves_tail_1), decltype(moves_2), N1 - 1, N2>::are_inverse(--level_1, 0, moves_tail_1, moves_2, inverse_funcs);
        }
        else
        {
          auto moves_tail_1 = tuple_tail(moves_1);
          auto moves_tail_2 = tuple_tail(moves_2);
          return BiMoveDispatcher<decltype(moves_tail_1), decltype(moves_tail_2), N1 - 1, N2 - 1>::are_inverse(--level_1, --level_2, moves_tail_1, moves_tail_2, inverse_funcs);
        }
      }
    };
    
    template <class MovesTuple1, class MovesTuple2, size_t N>
    struct BiMoveDispatcher<MovesTuple1, MovesTuple2, N, 0>
    {
      static bool are_inverse(long level_1, long level_2, const MovesTuple1 &moves_1, const MovesTuple2 &moves_2, const std::unordered_map<std::type_index, std::any> &inverse_funcs)
      {
        if (level_1 == 0 && level_2 == 0)
        {
          const auto &this_move_1 = std::get<0>(moves_1).get();
          const auto &this_move_2 = std::get<0>(moves_2).get();
          // searches whether there is an inverse for the combination of this_move_1 and this_move_2 in the registry
          auto it = inverse_funcs.find(std::type_index(typeid(std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>)));
          if (it != inverse_funcs.end()) // the inverse for the combination this_move_1 and this_move_2 is present
            // casts back the inverse function from the std::any type to the right one
            return std::any_cast<std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>>(it->second)(this_move_1, this_move_2);
          else // default inverse definition: if the moves have the same type then they should not be equal, otherwise they are not inverses
              return false;
        }
        else
          throw std::logic_error("End of tuple recursion");
      }
    };
    
    template <class MovesTuple1, class MovesTuple2, size_t N>
    struct BiMoveDispatcher<MovesTuple1, MovesTuple2, 0, N>
    {
      static bool are_inverse(long level_1, long level_2, const MovesTuple1 &moves_1, const MovesTuple2 &moves_2, const std::unordered_map<std::type_index, std::any> &inverse_funcs)
      {
        if (level_1 == 0 && level_2 == 0)
        {
          const auto &this_move_1 = std::get<0>(moves_1).get();
          const auto &this_move_2 = std::get<0>(moves_2).get();
          // searches whether there is an inverse for the combination of this_move_1 and this_move_2 in the registry
          auto it = inverse_funcs.find(std::type_index(typeid(std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>)));
          if (it != inverse_funcs.end()) // the inverse for the combination this_move_1 and this_move_2 is present
            // casts back the inverse function from the std::any type to the right one
            return std::any_cast<std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>>(it->second)(this_move_1, this_move_2);
          else // default inverse definition: if the moves have the same type then they should not be equal, otherwise they are not inverses
              return false;
        }
        else
          throw std::logic_error("End of tuple recursion");
      }
    };
    
    template <class MovesTuple1, class MovesTuple2>
    struct BiMoveDispatcher<MovesTuple1, MovesTuple2, 0, 0>
    {
      static bool are_inverse(long level_1, long level_2, const MovesTuple1 &moves_1, const MovesTuple2 &moves_2, const std::unordered_map<std::type_index, std::any> &inverse_funcs)
      {
        if (level_1 == 0 && level_2 == 0)
        {
          const auto &this_move_1 = std::get<0>(moves_1).get();
          const auto &this_move_2 = std::get<0>(moves_2).get();
          // searches whether there is an inverse for the combination of this_move_1 and this_move_2 in the registry
          auto it = inverse_funcs.find(std::type_index(typeid(std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>)));
          if (it != inverse_funcs.end()) // the inverse for the combination this_move_1 and this_move_2 is present
            // casts back the inverse function from the std::any type to the right one
            return std::any_cast<std::function<bool(const decltype(this_move_1) &, const decltype(this_move_2) &)>>(it->second)(this_move_1, this_move_2);
          else // default inverse definition: if the moves have the same type then they should not be equal, otherwise they are not inverses
              return false;
        }
        else
          throw std::logic_error("End of tuple recursion");
      }
    };
    } // namespace Impl
  
    /** Creates an array of a non default-constructible type */
    template <std::size_t N, typename T>
    constexpr std::array<T, N> make_array(const T& value)
    {
      return Impl::make_array(value, std::make_index_sequence<N>());
    }
    
    /** Given a set of base neighborhood explorers, this class will create a multimodal (i.e., compound) neighborhood explorer
     that explores the set union of all neighborhoods.
     @brief The SetUnion MultimodalNeighborhoodExplorer manages the set union of different neighborhoods.
     @tparam Input the class representing the problem input
     @tparam State the class representing the problem's state
     @tparam CFtype the type of the cost function
     @tparam BaseNeighborhoodExplorers a sequence of base neighborhood explorer classes
     @ingroup Helpers
     */
    template <class Input, class Solution, class CostStructure, class... BaseNeighborhoodExplorers>
    class SetUnionNeighborhoodExplorer : public NeighborhoodExplorer<Input, Solution, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>...>, CostStructure>
    {
    protected:
      /** Tuple type representing the combination of @c BaseNeighborhoodExplorers' @ref Move. */
      typedef std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>...> MoveTypes;
      
      /** Tuple type representing references to @c BaseNeighborhoodExplorers' @ref Move (because we need to set them). */
      typedef std::tuple<std::reference_wrapper<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>>...> MoveTypeRefs;            
      
      /** Tuple type representing const references to @c BaseNeighborhoodExplorers' @ref Move. */
      typedef std::tuple<std::reference_wrapper<const ActiveMove<typename BaseNeighborhoodExplorers::MoveType>>...> MoveTypeCRefs;
      
      /** Tuple type representing references to @c BaseNeighborhoodExplorers. */
      typedef std::tuple<std::reference_wrapper<BaseNeighborhoodExplorers>...> NeighborhoodExplorerTypes;
      
      /** Modality of the NeighborhoodExplorer, i.e., the number of @ref NeighborhoodExplorer composing this one.  */
      static constexpr size_t modality = sizeof...(BaseNeighborhoodExplorers);
      
    public:
      /** Constructor, takes a variable number of base NeighborhoodExplorers.
       @param in a pointer to an input object.
       @param sm a pointer to a compatible state manager.
       @param name the name associated to the NeighborhoodExplorer
       @param nhes the list of basic neighborhood explorer objects (according to the template list)
       @param bias a set of weights fo biasing the random move drawing
       */
      SetUnionNeighborhoodExplorer(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm, std::string name, BaseNeighborhoodExplorers &... nhes, std::array<double, modality> bias = std::array<double, modality>(0.0))
      : NeighborhoodExplorer<Input, Solution, MoveTypes, CostStructure>(in, sm, name),
      nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes)...)),
#ifndef MSVC
      // this uses fastfunc
      first_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFastFunc(dynamic_cast<NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#else
      first_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFunction3(dynamic_cast<NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#endif
      {
        if (std::all_of(begin(bias), end(bias), [](const double b) { return b == 0.0; }))
        {
          // If not otherwise specified, initialize the probabilities as 1 / modality
          double e = 1.0 / (double)modality;
          this->bias = make_array<modality>(e);
        }
        else
          this->bias = bias;
      }
      
      /** Retuns the modality of the neighborhood explorer, i.e., the number of different kind of moves handled by it.
       */
      size_t Modality() const { return modality; }
      
    protected:
      /** Instantiated base NeighborhoodExplorers. */
      NeighborhoodExplorerTypes nhes;
      
#ifndef MSVC
      typedef std::tuple<Impl::FastFunc<void(const Solution &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_ConstSolution_Move;
      typedef std::tuple<Impl::FastFunc<bool(const Solution &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Bool_ConstSolution_Move;
      typedef std::tuple<Impl::FastFunc<void(Solution&, const typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_Solution_ConstMove;
      typedef std::tuple<Impl::FastFunc<CostStructure(const Solution &, const typename BaseNeighborhoodExplorers::MoveType &, const std::vector<double> &weights)>...> _CostStructure_ConstSolution_ConstMove;

#else
      typedef std::tuple<std::function<void(const Solution &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_ConstState_Move;
      typedef std::tuple<std::function<bool(const Solution &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Bool_ConstState_Move;
      typedef std::tuple<std::function<void(Solution&, const typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_State_ConstMove;
      typedef std::tuple<std::function<CostStructure(const Solution &, const typename BaseNeighborhoodExplorers::MoveType &, const std::vector<double> &weights)>...> _CostStructure_ConstState_ConstMove;
#endif
      _Void_ConstSolution_Move first_move_funcs, random_move_funcs;
      _Bool_ConstSolution_Move next_move_funcs;
      _Void_Solution_ConstMove make_move_funcs;
      _CostStructure_ConstSolution_ConstMove delta_cost_function_funcs;
      
      std::array<double, modality> bias;
      
      // this is the registry of inverse functions
      std::unordered_map<std::type_index, std::any> inverse_funcs;
      
      typedef std::function<bool(const MoveTypes &lm, const MoveTypes &mv)> InverseFunctionType;
      
    public:
      
      /** Adds a predicate to determine whether two moves (possibly of different neighborhoods) are one the inverse of the other.
       This version wraps the moves in an ActiveMove object structure.
       @param r an inverse function
       */
      template <typename Move1, typename Move2>
      void AddInverseFunction(std::function<bool(const Move1 &, const Move2 &)> inverse)
      {
        std::function<bool(const ActiveMove<Move1> &, const ActiveMove<Move2> &)> ai = [inverse](const ActiveMove<Move1> &mv1, const ActiveMove<Move2> &mv2) { return inverse(mv1, mv2); };
        inverse_funcs[std::type_index(typeid(ai))] = ai;
      }
      
      /** @copydoc NeighborhoodExplorer::FirstMove */
      virtual void FirstMove(const Solution &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        
        for (size_t i = 0; i < modality; i++)
        {
          try
          {
            Impl::VTupleDispatcher<Solution, _Void_ConstSolution_Move, MoveTypeRefs, modality - 1>::execute_at(i, st, first_move_funcs, r_moves);
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, true);
            return;
          }
          catch (EmptyNeighborhood&)
          {
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, false);
          }
        }
        throw EmptyNeighborhood();
      }
      
      /** @copydoc NeighborhoodExplorer::RandomMove */
      virtual void RandomMove(const Solution &st, MoveTypes &moves) const
      {
        // transforms the reference to a tuple of moves to a tuple of references to moves
        MoveTypeRefs r_moves = to_refs(moves);
        Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_all_activity(r_moves, false);
        
        // Select random neighborhood explorer with bias (don't assume that they sum up to one)
        double total_bias = 0.0;
        double pick;
        unsigned int selected = 0;
        
        for (size_t i = 0; i < bias.size(); i++)
          total_bias += bias[i];
        pick = Random::Uniform<double>(0.0, total_bias);
        
        // Subtract bias until we're on the right neighborhood explorer
        while (pick > bias[selected])
        {
          pick -= bias[selected];
          selected++;
        }
        
        // TODO: currently it starts from the selected neighborhood and searches for the first that has some move afterwards
        for (size_t i = selected; i < modality; i++)
        {
          try
          {
            Impl::VTupleDispatcher<Solution, _Void_ConstSolution_Move, MoveTypeRefs, modality - 1>::execute_at(i, st, random_move_funcs, r_moves);
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, true);
            return;
          }
          catch (EmptyNeighborhood&)
          {
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, false);
          }
        }
        // TODO: restarting from the first neighborhood if needed
        for (size_t i = 0; i < selected; i++)
        {
          try
          {
            Impl::VTupleDispatcher<Solution, _Void_ConstSolution_Move, MoveTypeRefs, modality - 1>::execute_at(i, st, random_move_funcs, r_moves);
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, true);
            return;
          }
          catch (EmptyNeighborhood&)
          {
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, false);
          }
        }
        throw EmptyNeighborhood();
      }
      
      /** @copydoc NeighborhoodExplorer::NextMove */
      virtual bool NextMove(const Solution &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        
        size_t i = Impl::MoveDispatcher<MoveTypeCRefs, modality - 1>::get_first_active(cr_moves, 0);
        bool next_move_exists = false;
        
        while (true)
        {
          next_move_exists = Impl::TupleDispatcher<bool, Solution, _Bool_ConstSolution_Move, MoveTypeRefs, modality - 1>::execute_at(i, st, next_move_funcs, r_moves);
          if (next_move_exists)
          {
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, true);
            return true;
          }
          else
          {
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, false);
            for (i = i + 1; i < modality; i++)
            {
              try
              {
                Impl::VTupleDispatcher<Solution, _Void_ConstSolution_Move, MoveTypeRefs, modality - 1>::execute_at(i, st, first_move_funcs, r_moves);
                Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, true);
                return true;
              }
              catch (EmptyNeighborhood&)
              {
                Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(i, r_moves, false);
              }
            }
            return false;
          }
        }
        return false;
      }
      
      /** @copydoc NeighborhoodExplorer::MakeMove */
      virtual void MakeMove(Solution&st, const MoveTypes &moves) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        size_t i = Impl::MoveDispatcher<MoveTypeCRefs, modality - 1>::get_first_active(cr_moves, 0);
        
        Impl::VTupleDispatcher<Solution, _Void_Solution_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(i, st, make_move_funcs, cr_moves);
      }
      
      /** @copydoc NeighborhoodExplorer::DeltaCostFunctionComponents */
      virtual CostStructure DeltaCostFunctionComponents(const Solution &st, const MoveTypes &moves, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        size_t i = Impl::MoveDispatcher<MoveTypeCRefs, modality - 1>::get_first_active(cr_moves, 0);
        return Impl::TupleDispatcher<CostStructure, Solution, _CostStructure_ConstSolution_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(i, st, delta_cost_function_funcs, cr_moves, weights);
      }
      
      /** Returns an inverse function for Tabu Search */
      InverseFunctionType InverseFunction() const
      {
        return [this](const MoveTypes& lm, const MoveTypes& mv) -> bool {
          size_t i = Impl::MoveDispatcher<MoveTypeCRefs, modality - 1>::get_first_active(lm, 0),
          j = Impl::MoveDispatcher<MoveTypeCRefs, modality - 1>::get_first_active(mv, 0);
          return Impl::BiMoveDispatcher<MoveTypeCRefs, MoveTypeCRefs, modality - 1, modality - 1>::are_inverse(i, j, to_crefs(lm), to_crefs(mv), this->inverse_funcs);
        };
      }
    };
    
    /** Given a set of base neighborhood explorers, this class will create a multimodal (i.e., compound) neighborhood explorer
     that explores the set union of all neighborhoods.
     @brief The SetUnion MultimodalNeighborhoodExplorer manages the set union of different neighborhoods.
     @tparam Input the class representing the problem input
     @tparam State the class representing the problem's state
     @tparam CFtype the type of the cost function
     @tparam BaseNeighborhoodExplorers a sequence of base neighborhood explorer classes
     @ingroup Helpers
     */
    template <class Input, class Solution, class CostStructure, class... BaseNeighborhoodExplorers>
    class CartesianProductNeighborhoodExplorer : public NeighborhoodExplorer<Input, Solution, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>...>, CostStructure>
    {
    protected:
      /** Tuple type representing the combination of @c BaseNeighborhoodExplorers' @ref Move. */
      typedef std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>...> MoveTypes;
      
      /** Tuple type representing references to @c BaseNeighborhoodExplorers' @ref Move (because we need to set them). */
      typedef std::tuple<std::reference_wrapper<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>>...> MoveTypeRefs;
      
      /** Tuple type representing const references to @c BaseNeighborhoodExplorers' @ref Move. */
      typedef std::tuple<std::reference_wrapper<const ActiveMove<typename BaseNeighborhoodExplorers::MoveType>>...> MoveTypeCRefs;
      
      /** Tuple type representing references to @c BaseNeighborhoodExplorers. */
      typedef std::tuple<std::reference_wrapper<BaseNeighborhoodExplorers>...> NeighborhoodExplorerTypes;
      
      /** Modality of the NeighborhoodExplorer, i.e., the number of @ref NeighborhoodExplorer composing this one.  */
      static constexpr size_t modality = sizeof...(BaseNeighborhoodExplorers);
      
    public:
      typedef typename CostStructure::CFtype CFtype;
      
      /** Constructor, takes a variable number of base NeighborhoodExplorers.
       @param in a pointer to an input object.
       @param sm a pointer to a compatible state manager.
       @param name the name associated to the NeighborhoodExplorer
       @param nhes the list of basic neighborhood explorer objects (according to the template list)
       */
      CartesianProductNeighborhoodExplorer(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm, std::string name, BaseNeighborhoodExplorers &... nhes)
      : NeighborhoodExplorer<Input, Solution, MoveTypes, CostStructure>(in, sm, name),
      nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes)...)),
#ifndef MSVC
      first_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFastFunc(dynamic_cast<NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#else
      first_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFunction3(dynamic_cast<NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, Solution, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#endif
      {}
      
      /** Retuns the modality of the neighborhood explorer, i.e., the number of different kind of moves handled by it.
       */
      virtual size_t Modality() const
      {
        auto sum = std::apply([](auto&&... args){ return ( args.get().Modality() + ... ); }, nhes);
        return sum;
      }
      
    protected:
      /** Instantiated base NeighborhoodExplorers. */
      NeighborhoodExplorerTypes nhes;
      
      typedef std::tuple<Impl::FastFunc<void(const Solution &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_ConstState_Move;
      typedef std::tuple<Impl::FastFunc<bool(const Solution &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Bool_ConstState_Move;
      typedef std::tuple<Impl::FastFunc<void(Solution&, const typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_State_ConstMove;
      typedef std::tuple<Impl::FastFunc<CostStructure(const Solution &, const typename BaseNeighborhoodExplorers::MoveType &, const std::vector<double> &weights)>...> _CostStructure_ConstState_ConstMove;
      
      _Void_ConstState_Move first_move_funcs, random_move_funcs;
      _Bool_ConstState_Move next_move_funcs;
      _Void_State_ConstMove make_move_funcs;
      _CostStructure_ConstState_ConstMove delta_cost_function_funcs;
      
      // this is the registry of related functions
      std::unordered_map<std::type_index, std::any> related_funcs;
      // this is the registry of inverse functions
      std::unordered_map<std::type_index, std::any> inverse_funcs;
      
      typedef std::function<bool(const MoveTypes &lm, const MoveTypes &mv)> InverseFunctionType;

      
    public:
      /** Adds a predicate to determine whether two moves (of different neighborhoods) are related.
       This version wraps the moves in an ActiveMove object structure.
       @param r a relatedness function
       */
      template <typename Move1, typename Move2>
      void AddRelatedFunction(std::function<bool(const Move1 &, const Move2 &)> related)
      {
        std::function<bool(const ActiveMove<Move1> &, const ActiveMove<Move2> &)> ar = [related](const ActiveMove<Move1> &mv1, const ActiveMove<Move2> &mv2) { return related(mv1, mv2); };
        related_funcs[std::type_index(typeid(ar))] = ar;
      }
      
      /** Adds a predicate to determine whether two moves (possibly of different neighborhoods) are one the inverse of the other.
       This version wraps the moves in an ActiveMove object structure.
       @param r an inverse function
       */
      template <typename Move1, typename Move2>
      void AddInverseFunction(std::function<bool(const Move1 &, const Move2 &)> inverse)
      {
        std::function<bool(const ActiveMove<Move1> &, const ActiveMove<Move2> &)> ai = [inverse](const ActiveMove<Move1> &mv1, const ActiveMove<Move2> &mv2) { return inverse(mv1, mv2); };
        inverse_funcs[std::type_index(typeid(ai))] = ai;
      }
      
      /** @copydoc NeighborhoodExplorer::FirstMove */
      virtual void FirstMove(const Solution &st, MoveTypes &moves) const
      {
        // TODO: possibly remove r_moves and cr_moves by properly use static casting in Dispatcher Function (see state)
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        const size_t length = modality;
        std::array<Solution, modality> states = make_array<modality>(st);
        
        long cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
          
          // reset state before generating each move
          states[cur] = cur > 0 ? states[cur - 1] : st;
          
          if (!backtracking)
          {
            try
            {
              // ne.FirstMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<Solution, _Void_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, static_cast<const Solution&>(states[cur]), first_move_funcs, r_moves);
              
              while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::are_related(cur - 1, r_moves, related_funcs))
              {
                //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                if (!Impl::TupleDispatcher<bool, Solution, _Bool_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
                {
                  backtracking = true;
                  Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              // ne.MakeMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
              Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, true);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood& e)
            {
              backtracking = true;
              Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              //                if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              if (!Impl::TupleDispatcher<bool, Solution, _Bool_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
              {
                backtracking = true;
                Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
                cur--;
                goto loop;
              }
            } while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::are_related(cur - 1, r_moves, related_funcs));
            backtracking = false;
            //ne.MakeMove(kick[cur].second, kick[cur].first.move);
            Impl::VTupleDispatcher<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, true);
            cur++;
            goto loop;
          }
        }
      }
      
      /** @copydoc NeighborhoodExplorer::RandomMove */
      virtual void RandomMove(const Solution &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        const size_t length = modality;
        std::array<Solution, modality> states = make_array<modality>(st);
        
        MoveTypes initial_moves;
        MoveTypeRefs r_initial_moves = to_refs(initial_moves);
        //const MoveTypeCRefs cr_initial_moves = to_crefs(initial_moves);
        std::array<bool, modality> initial_set = {false};
        
        long cur = 0;
        bool backtracking = false;
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            throw EmptyNeighborhood();
          
          // reset state before generating each move
          states[cur] = cur > 0 ? states[cur - 1] : st;
          
          if (!backtracking)
          {
            try
            {
              //ne.RandomMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<Solution, _Void_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, static_cast<const Solution&>(states[cur]), random_move_funcs, r_moves);
              
              if (!initial_set[cur])
              {
                Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::copy_move_at(cur, r_initial_moves, r_moves);
                initial_set[cur] = true;
              }
              
              while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::are_related(cur - 1, r_moves, related_funcs))
              {
                //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                if (!Impl::TupleDispatcher<bool, Solution, _Bool_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
                {
                  // ne.FirstMove(kick[cur].second, kick[cur].first.move);
                  Impl::VTupleDispatcher<Solution, _Void_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, static_cast<const Solution&>(states[cur]), first_move_funcs, r_moves);
                }
                //                  if (kick[cur].first.move == initial_kick_moves[cur])
                if (Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::equal_at(cur, r_moves, r_initial_moves))
                {
                  backtracking = true;
                  Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              // ne.MakeMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
              Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, true);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood& e)
            {
              backtracking = true;
              Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate moves following the first)
          {
            do
            {
              //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              if (!Impl::TupleDispatcher<bool, Solution, _Bool_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
              {
                // ne.FirstMove(kick[cur].second, kick[cur].first.move);
                Impl::VTupleDispatcher<Solution, _Void_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, static_cast<const Solution&>(states[cur]), first_move_funcs, r_moves);
              }
              // if (kick[cur].first.move == initial_kick_moves[cur])
              if (Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::equal_at(cur, r_moves, r_initial_moves))
              {
                backtracking = true;
                Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
                cur--;
                goto loop;
              }
            } while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::are_related(cur - 1, r_moves, related_funcs));
            backtracking = false;
            //ne.MakeMove(kick[cur].second, kick[cur].first.move);
            Impl::VTupleDispatcher<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, true);
            cur++;
            goto loop;
          }
        }
      }
      
      /** @copydoc NeighborhoodExplorer::NextMove */
      virtual bool NextMove(const Solution &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        const size_t length = modality;
        std::array<Solution, modality> states = make_array<modality>(st);
        // go to last move, then start generating with backtracking
        long cur = length - 1;
        bool backtracking = true;
        // create a coherent initial sequence of states for the next moves
        for (size_t i = 0; i < length - 1; i++)
        {
          Impl::VTupleDispatcher<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(i, states[i], make_move_funcs, cr_moves);
        }
        
        // stop only when a complete kicker has been generated, or throw an @ref EmptyNeighborhood
      loop:
        while (cur < (int)length)
        {
          if (cur == -1)
            return false;
          
          // reset state before generating each move
          states[cur] = cur > 0 ? states[cur - 1] : st;
          
          if (!backtracking)
          {
            try
            {
              //ne.FirstMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<Solution, _Void_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, static_cast<const Solution&>(states[cur]), first_move_funcs, r_moves);
              while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::are_related(cur - 1, r_moves, related_funcs)) //!RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move))
              {
                //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                if (!Impl::TupleDispatcher<bool, Solution, _Bool_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, static_cast<const Solution&>(states[cur]), next_move_funcs, r_moves))
                {
                  backtracking = true;
                  Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              //ne.MakeMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
              Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, true);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood& e)
            {
              backtracking = true;
              Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              if (!Impl::TupleDispatcher<bool, Solution, _Bool_ConstState_Move, MoveTypeRefs, modality - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
              {
                backtracking = true;
                Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, false);
                cur--;
                goto loop;
              }
            } while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::are_related(cur - 1, r_moves, related_funcs)); //!RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move));
            backtracking = false;
            //ne.MakeMove(kick[cur].second, kick[cur].first.move);
            Impl::VTupleDispatcher<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
            Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::set_activity_at(cur, r_moves, true);
            cur++;
            goto loop;
          }
        }
        return true;
      }
      
      /** @copydoc NeighborhoodExplorer::MakeMove */
      virtual void MakeMove(Solution&st, const MoveTypes &moves) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        
        Impl::TupleApplier<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::apply_chain(st, make_move_funcs, cr_moves);
      }
      
      /** @copydoc NeighborhoodExplorer::DeltaCostFunctionComponents */
      virtual CostStructure DeltaCostFunctionComponents(const Solution &st, const MoveTypes &moves, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        
        Solution st1 = st;
        
        // TODO: probably it is faster to simulate the move and compute the difference
        // execute the move and compute the difference
        Impl::TupleApplier<Solution, _Void_State_ConstMove, MoveTypeCRefs, modality - 1>::apply_chain(st1, make_move_funcs, cr_moves);
        return this->sm.CostFunctionComponents(st1) - this->sm.CostFunctionComponents(st);        
      }
      
      /** Returns an inverse function for Tabu Search */
      InverseFunctionType InverseFunction() const
      {
        return [this](const MoveTypes& lm, const MoveTypes& mv) -> bool {
          size_t i = Impl::MoveDispatcher<MoveTypeCRefs, modality - 1>::get_first_active(lm, 0),
          j = Impl::MoveDispatcher<MoveTypeCRefs, modality - 1>::get_first_active(mv, 0);
          return Impl::MoveDispatcher<MoveTypeRefs, modality - 1>::are_inverse(i, j, mv, lm, this->inverse_funcs);
        };
      }
    };
  } // namespace Core
} // namespace EasyLocal
