#if !defined(_MULTIMODAL_TABU_LIST_MANAGER_HH_)
#define _MULTIMODAL_TABU_LIST_MANAGER_HH_

#include "easylocal/helpers/tabulistmanager.hh"
#include "easylocal/helpers/multimodalneighborhoodexplorer.hh"
#include "easylocal/utils/printable.hh"

namespace EasyLocal {

  namespace Core {

    /** Multimodal tabu list manager, to handle tabu lists of multimodal moves. */
    template <class State, typename CFtype, class ... BaseTabuListManagers>
    class MultimodalTabuListManager : public TabuListManager<State, std::tuple<ActiveMove<typename BaseTabuListManagers::MoveType> ...>, CFtype>, public Printable
    {
    public:

      /** Typedefs. */
      typedef std::tuple<ActiveMove<typename BaseTabuListManagers::MoveType> ...> MoveTypes;
      typedef TabuListManager<State, std::tuple<ActiveMove<typename BaseTabuListManagers::MoveType> ...>, CFtype> SuperTabuListManager; // type of this tabu list manager
      typedef std::tuple<BaseTabuListManagers...> TabuListManagerTypes;

      /** Modality of the TabuListManager */
      virtual unsigned int Modality() const { return sizeof...(BaseTabuListManagers); }

      /** Read all parameters from an input stream (prints hints on output stream). */
      virtual void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
      {
        ParametersDispatcher<TabuListManagerTypes, sizeof...(BaseTabuListManagers) - 1>::ReadParameters(tlms, is, os);
      }

      /** @copydoc Printable::Print() */
      virtual void Print(std::ostream& os = std::cout) const
      {
        ParametersDispatcher<TabuListManagerTypes, sizeof...(BaseTabuListManagers) - 1>::Print(const_cast<TabuListManagerTypes&>(tlms), os);
      }

      /** Queries for status all of its composing TabuListManagers. */
      virtual std::string StatusString() const
      {
        Call status_string(Call::Function::STATUS_STRING);
        return TupleDispatcher<MoveTypes, TabuListManagerTypes, sizeof...(BaseTabuListManagers)-1>::QueryAll(const_cast<TabuListManagerTypes&>(tlms), status_string);
      }

    protected:

      /** Constructor, takes a variable number of base TabuListManagers.  */
      MultimodalTabuListManager(BaseTabuListManagers& ... tlms) 
        : tlms(std::make_tuple(BaseTabuListManagers(tlms) ...)) { }

      /** List of tabu list managers. */
      TabuListManagerTypes tlms;

      /** Tuple dispatching helpers. */
      class Call
      {
      public:
        enum Function { IS_INVERSE, IS_ACTIVE, STATUS_STRING };
        Call(Function f) : to_call(f) { }

        template <class T, class M>
        std::function<bool(const T&, const M&, const M&)> getBool() const throw(std::logic_error)
        {
          std::function<bool(const T&, const M&, const M&)> f;
          switch (to_call)
          {
            case IS_INVERSE:
            f = &IsInverse<T, M>;
            break;
            case IS_ACTIVE:
            f = &IsActive<T, M>;
            break;
            default:
            throw std::logic_error("Function not implemented");
          }
          return f;
        }

        template <class T>
        std::function<std::string(const T&)> getString() const throw(std::logic_error)
        {
          std::function<std::string(const T&)> f;
          switch(to_call)
          {
            case STATUS_STRING:
            f = &GetStatusString<T>;
            break;
            default:
            throw std::logic_error("Function not implemented.");
          }
          return f;
        }

        Function to_call;
      };

      template <class TupleOfMoves, class TupleOfTLMs, std::size_t N>
      struct TupleDispatcher
      {
        static std::string QueryAll(const TupleOfTLMs& temp_tlms, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<N, TupleOfTLMs>::type CurrentTLM;

          const CurrentTLM& this_tlm = std::get<N>(temp_tlms);

          std::function<std::string(const CurrentTLM&)> f =  c.template getString<CurrentTLM>();

          std::string current, others;
          current = f(this_tlm);
          others = TupleDispatcher<TupleOfMoves, TupleOfTLMs, N - 1>::QueryAll(temp_tlms, c);
          current.append(", ");
          current.append(others);
          return current;
        }

        static std::vector<bool> Check(State& st, const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
        {
          // Get right types (be nice to gcc)
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
          // Get right types (be nice to gcc)
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
          // Get right types (be nice to gcc)
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
            // Get right types (be nice to gcc)
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
        static std::string QueryAll(const TupleOfTLMs& temp_tlms, const Call& c)
        {
          // Get right types (be nice to gcc)
          typedef typename std::tuple_element<0, TupleOfTLMs>::type CurrentTLM;
          const CurrentTLM& this_tlm = std::get<0>(temp_tlms);
          std::function<std::string(const CurrentTLM&)> f =  c.template getString<CurrentTLM>();
          return f(this_tlm);
        }

        static std::vector<bool> Check(const TupleOfMoves& temp_moves_1, const TupleOfMoves& temp_moves_2, const TupleOfTLMs& temp_tlms, const Call& c)
        {
          // Get right types (be nice to gcc)
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
          // Get right types (be nice to gcc)
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
          // Get right types (be nice to gcc)
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
            // Get right types (be nice to gcc)
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
      static std::vector<bool> Check(const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_2, const std::tuple<TLMs...>& tlms, const Call& c)
      {
        return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::MoveType>...>, std::tuple<TLMs...>, sizeof...(TLMs) - 1>::Check(moves_1, moves_2, tlms, c);
      }

      /** CheckAll - checks a predicate on all tuple */
      template <class ...TLMs>
      static bool CheckAll(const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_2, const std::tuple<TLMs...>& tlms, const Call& c)
      {
        return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::MoveType>...>, std::tuple<TLMs...>, sizeof...(TLMs) - 1>::CheckAll(moves_1, moves_2, tlms, c);
      }

      /** CheckAny - checks a predicate on all tuple */
      template <class ...TLMs>
      static bool CheckAny(const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_2, const std::tuple<TLMs...>& tlms, const Call& c)
      {
        return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::MoveType>...>, std::tuple<TLMs...>, sizeof...(TLMs) - 1>::CheckAny(moves_1, moves_2, tlms, c);
      }

      /** CheckAt - checks a predicate at a certain level of the tuple and returns its value */
      template <class ...TLMs>
      static CFtype CheckAt(const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_1, const std::tuple<ActiveMove<typename TLMs::MoveType>...>& moves_2, const std::tuple<TLMs...>& tlms, const Call& c, int iter)
      {
        return TupleDispatcher<std::tuple<ActiveMove<typename TLMs::MoveType>...>, std::tuple<TLMs...>, sizeof...(TLMs) - 1>::CheckAt(moves_1, moves_2, tlms, c, (sizeof...(TLMs) - 1) - iter);
      }

      template <class T, class M>
      static bool IsInverse(const T& tlm, const M& move_1, const M& move_2)
      {
        if (move_1.active && move_2.active)
        return tlm.Inverse(move_1, move_2);
        return false;
      }

      template <class T, class M>
      static bool IsActive(const T& tlm, const M& move_1, const M& move_2)
      {
        return move_1.active;
      }

      template <class T>
      static std::string GetStatusString(const T& tlm)
      {
        return tlm.StatusString();
      }

      template <typename T, std::size_t N>
      struct ParametersDispatcher
      {
        static void ReadParameters(T& tlms, std::istream& is, std::ostream& os)
        {
          typedef typename std::tuple_element<N, T>::type CurrentTLM;
          CurrentTLM& tlm = std::get<N>(tlms);
          tlm.ReadParameters(is, os);
          ParametersDispatcher<T, N - 1>::ReadParameters(tlms, is, os);
        }

        static void Print(T& tlms, std::ostream& os)
        {
          typedef typename std::tuple_element<N, T>::type CurrentTLM;
          CurrentTLM& tlm = std::get<N>(tlms);
          tlm.Print(os);
          ParametersDispatcher<T, N - 1>::Print(tlms, os);
        }
      };

      template<typename T>
      struct ParametersDispatcher<T, 0>
      {
        static void ReadParameters(T& tlms, std::istream& is, std::ostream& os)
        {
          typedef typename std::tuple_element<0, T>::type CurrentTLM;
          CurrentTLM& tlm = std::get<0>(tlms);
          tlm.ReadParameters(is, os);
        }

        static void Print(T& tlms, std::ostream& os)
        {
          typedef typename std::tuple_element<0, T>::type CurrentTLM;
          CurrentTLM& tlm = std::get<0>(tlms);
          tlm.Print(os);
        }
      };
    };

    template <class State, typename CFtype, class ... BaseTabuListManagers>
    class SetUnionTabuListManager : public MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>
    {
      typedef MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...> SuperTabuListManager;
      typedef typename SuperTabuListManager::Call Call;
    public:
      SetUnionTabuListManager(BaseTabuListManagers& ... tlms)
      : MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>(tlms...)
      {

      }
      virtual bool Inverse(const typename SuperTabuListManager::MoveType& moves_1, const typename SuperTabuListManager::MoveType& moves_2) const
      {
        Call is_inverse(Call::Function::IS_INVERSE);
        return SuperTabuListManager::CheckAny(moves_1, moves_2, this->tlms, is_inverse);
      }
    };

    template <class State, typename CFtype, class ... BaseTabuListManagers>
    class CartesianProductTabuListManager : public MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>
    {
      typedef MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...> SuperTabuListManager;
      typedef typename SuperTabuListManager::Call Call;
    public:
      CartesianProductTabuListManager(BaseTabuListManagers& ... tlms)
      : MultimodalTabuListManager<State, CFtype, BaseTabuListManagers...>(tlms...)
      {

      }
      virtual bool Inverse(const typename SuperTabuListManager::MoveType& moves_1, const typename SuperTabuListManager::MoveType& moves_2) const
      {
        Call is_inverse(Call::Function::IS_INVERSE);
        return SuperTabuListManager::CheckAll(moves_1, moves_2, this->tlms, is_inverse);
      }
    };
  }
}

#endif // _MULTIMODAL_TABU_LIST_MANAGER_HH_
