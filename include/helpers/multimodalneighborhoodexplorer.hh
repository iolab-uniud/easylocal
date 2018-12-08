#pragma once

#include "helpers/neighborhoodexplorer.hh"
#include <functional>
#include "utils/FastFunc.hh"
#include "utils/tuple.hh"

#include <boost/any.hpp>
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
    
    /* This namespace will hide impelementation components */
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
      
      /** Helper class for dispatching tuples and applying functions to tuple elements. General recursion case.
       * This version will handle procedures (i.e., functions returning void). */
      template <class State, class FuncsTuple, class MovesTuple, size_t N>
      struct VTupleDispatcher
      {
        static void execute_at(long level, const State &st, const FuncsTuple &funcs, MovesTuple &moves)
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
            VTupleDispatcher<State, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail);
          }
        }
        static void execute_at(long level, State &st, const FuncsTuple &funcs, const MovesTuple &moves)
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
            VTupleDispatcher<State, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail);
          }
        }
      };
      
      /** Helper class for dispatching tuples and applying functions to tuple elements. Base recursion case.
       * This version will handle procedures (i.e., functions returning void). */
      template <class State, class FuncsTuple, class MovesTuple>
      struct VTupleDispatcher<State, FuncsTuple, MovesTuple, 0>
      {
        static void execute_at(long level, const State &st, const FuncsTuple &funcs, MovesTuple &moves)
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
        static void execute_at(long level, State &st, const FuncsTuple &funcs, const MovesTuple &moves)
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
      template <class ReturnType, class State, class FuncsTuple, class MovesTuple, size_t N>
      struct TupleDispatcher
      {
        static ReturnType execute_at(long level, const State &st, const FuncsTuple &funcs, MovesTuple &moves)
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
            return TupleDispatcher<ReturnType, State, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail);
          }
        }
        static ReturnType execute_at(long level, const State &st, const FuncsTuple &funcs, const MovesTuple &moves, const std::vector<double> &weights)
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
            return TupleDispatcher<ReturnType, State, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail, moves_tail, weights);
          }
        }
        static ReturnType execute_at(long level, const State &st, const FuncsTuple &funcs)
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
            return TupleDispatcher<ReturnType, State, decltype(funcs_tail), decltype(moves_tail), N - 1>::execute_at(--level, st, funcs_tail);
          }
        }
      };
      /** Helper class for dispatching tuples and applying functions to tuple elements. Base recursion case.
       * This version will handle functions. */
      template <class ReturnType, class State, class FuncsTuple, class MovesTuple>
      struct TupleDispatcher<ReturnType, State, FuncsTuple, MovesTuple, 0>
      {
        static ReturnType execute_at(long level, const State &st, const FuncsTuple &funcs, MovesTuple &moves)
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
        static ReturnType execute_at(long level, const State &st, const FuncsTuple &funcs, const MovesTuple &moves, const std::vector<double> &weights)
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
        static ReturnType execute_at(long level, const State &st, const FuncsTuple &funcs)
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
            auto moves_1_tail = tuple_tail(moves_1);
            auto moves_2_tail = tuple_tail(moves_2);
            return MoveDispatcher<decltype(moves_1_tail), N - 1>::equal_at(--level, moves_1_tail, moves_2_tail);
          }
        }
        static bool are_related(long level, const MovesTuple &moves, const std::unordered_map<std::type_index, boost::any> &related_funcs)
        {
          if (level == 0)
          {
            const auto &this_move = std::get<0>(moves).get();
            const auto &next_move = std::get<1>(moves).get();
            auto it = related_funcs.find(std::type_index(typeid(std::function<bool(const decltype(this_move) &, const decltype(next_move) &)>)));
            if (it == related_funcs.end())
              return true;
            else
              return boost::any_cast<std::function<bool(const decltype(this_move) &, const decltype(next_move) &)>>(it->second)(this_move, next_move);
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
        static bool are_related(long level, const MovesTuple &moves, const std::unordered_map<std::type_index, boost::any> &related_funcs)
        {
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
    } // namespace Impl
    
    /** Given a set of base neighborhood explorers, this class will create a multimodal (i.e., compound) neighborhood explorer
     that explores the set union of all neighborhoods.
     @brief The SetUnion MultimodalNeighborhoodExplorer manages the set union of different neighborhoods.
     @tparam Input the class representing the problem input
     @tparam State the class representing the problem's state
     @tparam CFtype the type of the cost function
     @tparam BaseNeighborhoodExplorers a sequence of base neighborhood explorer classes
     @ingroup Helpers
     */
    template <class Input, class State, class CostStructure, class... BaseNeighborhoodExplorers>
    class SetUnionNeighborhoodExplorer : public NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>...>, CostStructure>
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
      virtual size_t Modality() const { return sizeof...(BaseNeighborhoodExplorers); }
      
    public:
      /** Constructor, takes a variable number of base NeighborhoodExplorers.
       @param in a pointer to an input object.
       @param sm a pointer to a compatible state manager.
       @param name the name associated to the NeighborhoodExplorer
       @param nhes the list of basic neighborhood explorer objects (according to the template list)
       @param bias a set of weights fo biasing the random move drawing
       */
      SetUnionNeighborhoodExplorer(const Input &in, StateManager<Input, State, CostStructure> &sm, std::string name, BaseNeighborhoodExplorers &... nhes, const std::vector<double> &bias = std::vector<double>(0))
      : NeighborhoodExplorer<Input, State, MoveTypes, CostStructure>(in, sm, name),
      nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes)...)),
#ifndef MSVC
      // this uses fastfunc
      first_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFastFunc(dynamic_cast<NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#else
      first_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFunction3(dynamic_cast<NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#endif
      {
        if (bias.empty())
        {
          // If not otherwise specified, initialize the probabilities as 1 / Modality
          this->bias = std::vector<double>(this->Modality(), 1.0 / (double)this->Modality());
        }
        
        else if (bias.size() != this->Modality())
          throw std::logic_error("Multimodal move random distribution (i.e., bias) not matching the neighborhood modality");
        else
          this->bias = bias;
      }
      
    protected:
      /** Instantiated base NeighborhoodExplorers. */
      NeighborhoodExplorerTypes nhes;
      
#ifndef MSVC
      typedef std::tuple<Impl::FastFunc<void(const State &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_ConstState_Move;
      typedef std::tuple<Impl::FastFunc<bool(const State &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Bool_ConstState_Move;
      typedef std::tuple<Impl::FastFunc<void(State &, const typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_State_ConstMove;
      typedef std::tuple<Impl::FastFunc<CostStructure(const State &, const typename BaseNeighborhoodExplorers::MoveType &, const std::vector<double> &weights)>...> _CostStructure_ConstState_ConstMove;
      
#else
      typedef std::tuple<std::function<void(const State &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_ConstState_Move;
      typedef std::tuple<std::function<bool(const State &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Bool_ConstState_Move;
      typedef std::tuple<std::function<void(State &, const typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_State_ConstMove;
      typedef std::tuple<std::function<CostStructure(const State &, const typename BaseNeighborhoodExplorers::MoveType &, const std::vector<double> &weights)>...> _CostStructure_ConstState_ConstMove;
#endif
      _Void_ConstState_Move first_move_funcs, random_move_funcs;
      _Bool_ConstState_Move next_move_funcs;
      _Void_State_ConstMove make_move_funcs;
      _CostStructure_ConstState_ConstMove delta_cost_function_funcs;
      
      std::vector<double> bias;
      
    public:
      /** @copydoc NeighborhoodExplorer::FirstMove */
      virtual void FirstMove(const State &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        
        for (size_t i = 0; i < Modality(); i++)
        {
          try
          {
            Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, first_move_funcs, r_moves);
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, true);
            return;
          }
          catch (EmptyNeighborhood)
          {
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, false);
          }
        }
        throw EmptyNeighborhood();
      }
      
      /** @copydoc NeighborhoodExplorer::RandomMove */
      virtual void RandomMove(const State &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_all_activity(r_moves, false);
        
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
        
        for (size_t i = selected; i < Modality(); i++)
        {
          try
          {
            Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, random_move_funcs, r_moves);
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, true);
            return;
          }
          catch (EmptyNeighborhood)
          {
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, false);
          }
        }
        for (size_t i = 0; i < selected; i++)
        {
          try
          {
            Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, random_move_funcs, r_moves);
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, true);
            return;
          }
          catch (EmptyNeighborhood)
          {
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, false);
          }
        }
        throw EmptyNeighborhood();
      }
      
      /** @copydoc NeighborhoodExplorer::NextMove */
      virtual bool NextMove(const State &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        
        size_t i = Impl::MoveDispatcher<MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::get_first_active(cr_moves, 0);
        bool next_move_exists = false;
        
        while (true)
        {
          next_move_exists = Impl::TupleDispatcher<bool, State, _Bool_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, next_move_funcs, r_moves);
          if (next_move_exists)
          {
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, true);
            return true;
          }
          else
          {
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, false);
            for (i = i + 1; i < Modality(); i++)
            {
              try
              {
                Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, first_move_funcs, r_moves);
                Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, true);
                return true;
              }
              catch (EmptyNeighborhood)
              {
                Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(i, r_moves, false);
              }
            }
            return false;
          }
        }
        return false;
      }
      
      /** @copydoc NeighborhoodExplorer::MakeMove */
      virtual void MakeMove(State &st, const MoveTypes &moves) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        size_t i = Impl::MoveDispatcher<MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::get_first_active(cr_moves, 0);
        
        Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, make_move_funcs, cr_moves);
      }
      
      /** @copydoc NeighborhoodExplorer::DeltaCostFunctionComponents */
      virtual CostStructure DeltaCostFunctionComponents(const State &st, const MoveTypes &moves, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        size_t i = Impl::MoveDispatcher<MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::get_first_active(cr_moves, 0);
        return Impl::TupleDispatcher<CostStructure, State, _CostStructure_ConstState_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, delta_cost_function_funcs, cr_moves, weights);
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
    template <class Input, class State, class CostStructure, class... BaseNeighborhoodExplorers>
    class CartesianProductNeighborhoodExplorer : public NeighborhoodExplorer<Input, State, std::tuple<ActiveMove<typename BaseNeighborhoodExplorers::MoveType>...>, CostStructure>
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
      virtual size_t Modality() const { return sizeof...(BaseNeighborhoodExplorers); }
      
    public:
      typedef typename CostStructure::CFtype CFtype;
      
      /** Constructor, takes a variable number of base NeighborhoodExplorers.
       @param in a pointer to an input object.
       @param sm a pointer to a compatible state manager.
       @param name the name associated to the NeighborhoodExplorer
       @param nhes the list of basic neighborhood explorer objects (according to the template list)
       */
      CartesianProductNeighborhoodExplorer(const Input &in, StateManager<Input, State, CostStructure> &sm, std::string name, BaseNeighborhoodExplorers &... nhes)
      : NeighborhoodExplorer<Input, State, MoveTypes, CostStructure>(in, sm, name),
      nhes(std::make_tuple(std::reference_wrapper<BaseNeighborhoodExplorers>(nhes)...)),
#ifndef MSVC
      first_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFastFunc(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFastFunc(dynamic_cast<NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#else
      first_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::FirstMove)...)),
      random_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::RandomMove)...)),
      next_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::NextMove)...)),
      make_move_funcs(std::make_tuple(Impl::makeFunction2(&nhes, &BaseNeighborhoodExplorers::MakeMove)...)),
      delta_cost_function_funcs(std::make_tuple(Impl::makeFunction3(dynamic_cast<NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure> *>(&nhes), &NeighborhoodExplorer<Input, State, typename BaseNeighborhoodExplorers::MoveType, CostStructure>::DeltaCostFunctionComponents)...))
#endif
      {
      }
      
    protected:
      /** Instantiated base NeighborhoodExplorers. */
      NeighborhoodExplorerTypes nhes;
      
      typedef std::tuple<Impl::FastFunc<void(const State &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_ConstState_Move;
      typedef std::tuple<Impl::FastFunc<bool(const State &, typename BaseNeighborhoodExplorers::MoveType &)>...> _Bool_ConstState_Move;
      typedef std::tuple<Impl::FastFunc<void(State &, const typename BaseNeighborhoodExplorers::MoveType &)>...> _Void_State_ConstMove;
      typedef std::tuple<Impl::FastFunc<CostStructure(const State &, const typename BaseNeighborhoodExplorers::MoveType &, const std::vector<double> &weights)>...> _CostStructure_ConstState_ConstMove;
      
      _Void_ConstState_Move first_move_funcs, random_move_funcs;
      _Bool_ConstState_Move next_move_funcs;
      _Void_State_ConstMove make_move_funcs;
      _CostStructure_ConstState_ConstMove delta_cost_function_funcs;
      
      std::unordered_map<std::type_index, boost::any> related_funcs;
      
    public:
      /** Adds a predicate to determine whether two moves (of different neighborhoods) are related.
       This version wraps the moves in an ActiveMove object structure.
       @param r a relatedness function
       */
      template <typename Move1, typename Move2>
      void AddRelatedFunction(std::function<bool(const Move1 &, const Move2 &)> r)
      {
        std::function<bool(const ActiveMove<Move1> &, const ActiveMove<Move2> &)> ar = [r](const ActiveMove<Move1> &mv1, const ActiveMove<Move2> &mv2) { return r(mv1, mv2); };
        related_funcs[std::type_index(typeid(ar))] = ar;
      }
      
      /** Adds a predicate to determine whether two moves (of different neighborhoods) are related.
       This version requires the moves to be already wrapped in an ActiveMove object structure.
       @param r a relatedness function
       */
      template <typename Move1, typename Move2>
      void AddRelatedFunction(std::function<bool(const ActiveMove<Move1> &, const ActiveMove<Move2> &)> r)
      {
        related_funcs[std::type_index(typeid(r))] = r;
      }
      
      /** @copydoc NeighborhoodExplorer::FirstMove */
      virtual void FirstMove(const State &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        const size_t length = sizeof...(BaseNeighborhoodExplorers);
        std::vector<State> states(length, st);
        
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
              Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, st, first_move_funcs, r_moves);
              
              while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::are_related(cur - 1, r_moves, related_funcs))
              {
                //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                if (!Impl::TupleDispatcher<bool, State, _Bool_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
                {
                  backtracking = true;
                  Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              // ne.MakeMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
              Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, true);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              //                if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              if (!Impl::TupleDispatcher<bool, State, _Bool_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
              {
                backtracking = true;
                Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
                cur--;
                goto loop;
              }
            } while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::are_related(cur - 1, r_moves, related_funcs));
            backtracking = false;
            //ne.MakeMove(kick[cur].second, kick[cur].first.move);
            Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, true);
            cur++;
            goto loop;
          }
        }
      }
      
      /** @copydoc NeighborhoodExplorer::RandomMove */
      virtual void RandomMove(const State &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        const size_t length = sizeof...(BaseNeighborhoodExplorers);
        std::vector<State> states(length, st);
        
        MoveTypes initial_moves;
        MoveTypeRefs r_initial_moves = to_refs(initial_moves);
        //const MoveTypeCRefs cr_initial_moves = to_crefs(initial_moves);
        std::vector<bool> initial_set(length, false);
        
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
              const State &c_cur_st = states[cur];
              //ne.RandomMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, c_cur_st, random_move_funcs, r_moves);
              
              if (!initial_set[cur])
              {
                Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::copy_move_at(cur, r_initial_moves, r_moves);
                initial_set[cur] = true;
              }
              
              while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::are_related(cur - 1, r_moves, related_funcs))
              {
                //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                if (!Impl::TupleDispatcher<bool, State, _Bool_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
                {
                  // ne.FirstMove(kick[cur].second, kick[cur].first.move);
                  Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, st, first_move_funcs, r_moves);
                }
                //                  if (kick[cur].first.move == initial_kick_moves[cur])
                if (Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::equal_at(cur, r_moves, r_initial_moves))
                {
                  backtracking = true;
                  Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              // ne.MakeMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
              Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, true);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate moves following the first)
          {
            do
            {
              //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              if (!Impl::TupleDispatcher<bool, State, _Bool_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
              {
                // ne.FirstMove(kick[cur].second, kick[cur].first.move);
                Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, st, first_move_funcs, r_moves);
              }
              // if (kick[cur].first.move == initial_kick_moves[cur])
              if (Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::equal_at(cur, r_moves, r_initial_moves))
              {
                backtracking = true;
                Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
                cur--;
                goto loop;
              }
            } while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::are_related(cur - 1, r_moves, related_funcs));
            backtracking = false;
            //ne.MakeMove(kick[cur].second, kick[cur].first.move);
            Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, true);
            cur++;
            goto loop;
          }
        }
      }
      
      /** @copydoc NeighborhoodExplorer::NextMove */
      virtual bool NextMove(const State &st, MoveTypes &moves) const
      {
        MoveTypeRefs r_moves = to_refs(moves);
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        const size_t length = sizeof...(BaseNeighborhoodExplorers);
        std::vector<State> states(length, st);
        // go to last move, then start generating with backtracking
        long cur = length - 1;
        bool backtracking = true;
        
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
              Impl::VTupleDispatcher<State, _Void_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, st, first_move_funcs, r_moves);
              while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::are_related(cur - 1, r_moves, related_funcs)) //!RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move))
              {
                //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
                if (!Impl::TupleDispatcher<bool, State, _Bool_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
                {
                  backtracking = true;
                  Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
                  cur--;
                  goto loop;
                }
              }
              backtracking = false;
              //ne.MakeMove(kick[cur].second, kick[cur].first.move);
              Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
              Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, true);
              cur++;
              goto loop;
            }
            catch (EmptyNeighborhood e)
            {
              backtracking = true;
              Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
              cur--;
              goto loop;
            }
          }
          else // backtracking (we only need to generate NextMoves)
          {
            do
            {
              //if (!ne.NextMove(kick[cur].second, kick[cur].first.move))
              if (!Impl::TupleDispatcher<bool, State, _Bool_ConstState_Move, MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], next_move_funcs, r_moves))
              {
                backtracking = true;
                Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, false);
                cur--;
                goto loop;
              }
            } while (cur > 0 && !Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::are_related(cur - 1, r_moves, related_funcs)); //!RelatedMoves(kick[cur - 1].first.move, kick[cur].first.move));
            backtracking = false;
            //ne.MakeMove(kick[cur].second, kick[cur].first.move);
            Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(cur, states[cur], make_move_funcs, cr_moves);
            Impl::MoveDispatcher<MoveTypeRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::set_activity_at(cur, r_moves, true);
            cur++;
            goto loop;
          }
        }
        return true;
      }
      
      /** @copydoc NeighborhoodExplorer::MakeMove */
      virtual void MakeMove(State &st, const MoveTypes &moves) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        
        for (size_t i = 0; i < Modality(); i++)
          Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, st, make_move_funcs, cr_moves);
      }
      
      /** @copydoc NeighborhoodExplorer::DeltaCostFunctionComponents */
      virtual CostStructure DeltaCostFunctionComponents(const State &st, const MoveTypes &moves, const std::vector<double> &weights = std::vector<double>(0)) const
      {
        const MoveTypeCRefs cr_moves = to_crefs(moves);
        
        CostStructure result;
        const size_t length = sizeof...(BaseNeighborhoodExplorers);
        std::vector<State> states(length, st);
        for (size_t i = 0; i < Modality(); i++)
        {
          states[i] = i > 0 ? states[i - 1] : st;
          result += Impl::TupleDispatcher<CostStructure, State, _CostStructure_ConstState_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, states[i], delta_cost_function_funcs, cr_moves, weights);
          Impl::VTupleDispatcher<State, _Void_State_ConstMove, MoveTypeCRefs, sizeof...(BaseNeighborhoodExplorers) - 1>::execute_at(i, states[i], make_move_funcs, cr_moves);
        }
        
        return result;
      }
    };
  } // namespace Core
} // namespace EasyLocal
