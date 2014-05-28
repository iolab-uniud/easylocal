#if !defined(_TUPLE_HH_)
#define _TUPLE_HH_

namespace EasyLocal {

  namespace Core {
  
    /** Template struct whose parameters are types from 0 to N-1 (tail indices). */
    template <int ...>
    struct tuple_index { };

    /** Struct to generate tail_index given a certain N-1. */
    template <int N, int ... S>
    struct make_index : make_index<N-1, N-1, S ...> { };

    /** Struct to generate tail_index given a certain N, base case. */
    template <int ... S>
    struct make_index<0, S ...>
    {
      typedef tuple_index<S ...> type;
    };

    /** Template struct whose parameters are types from 1 to N-1 (tail indices). */
    template <int ...>
    struct tail_index { };

    /** Struct to generate tail_index given a certain N. */
    template <int N, int ... S>
    struct make_tail : make_tail<N-1, N-1, S ...> { };

    /** Struct to generate tail_index given a certain N, base case. */
    template <int ... S>
    struct make_tail<1, S ...>
    {
      typedef tail_index<S ...> type;
    };

    /** Generates a tuple's tail. */
    template <typename H, typename ... T>
    std::tuple<T...> tuple_tail(const std::tuple<H, T...>& original)
    {
      return tuple_tail(typename make_tail<std::tuple_size<std::tuple<H,T...>>::value>::type(), original);
    }

    /** Generates a tuple's tail, with reference wrappers. */
    template <typename H, typename ... T>
    std::tuple<std::reference_wrapper<T>...> tuple_tail(const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T>...>& original)
    {
      return tuple_tail(typename make_tail<std::tuple_size<std::tuple<H,T...>>::value>::type(), original);
    }

    /** Generates a tuple's tail, case base. */
    template <typename H, typename ... T, int ... S>
    std::tuple<T...> tuple_tail(tail_index<S...>, const std::tuple<H,T...>& original)
    {
      return std::make_tuple(std::get<S>(original) ...);
    }

    /** Generates a tuple's tail, case base with reference wrappers. */
    template <typename H, typename ... T, int ... S>
    std::tuple<std::reference_wrapper<T>...> tuple_tail(tail_index<S...>, const std::tuple<std::reference_wrapper<H>, std::reference_wrapper<T>...>& original)
    {
      return std::make_tuple(std::get<S>(original) ...);
    }

    /** Prints a generic tuple. */
    template <unsigned int I>
    struct print_tuple {
      template <typename Tuple>
      static void print(std::ostream& os, const Tuple& t)
      {
        print_tuple<I-1>::print(os, t);
        os << std::get<I>(t) << " | ";
      }
    };

    template<>
    struct print_tuple<0>
    {
      template <typename Tuple>
      static void print(std::ostream& os, const Tuple& t)
      {
        os << "| " << std::get<0>(t) << " | ";
      }
    };

    /* template <typename H, typename S, typename ...T>
    void print_tuple(std::ostream& os, const std::tuple<H, S, T...>& t)
    {
    os << std::get<0>(t) << "-";
    auto temp_tuple_tail = tuple_tail(t);
    print_tuple<S, T...>(os, temp_tuple_tail);
    } */

    /** Prints a generic tuple, base case. */
    /* template <typename H>
    void print_tuple(std::ostream& os, const std::tuple<H>& t)
    {
    os << std::get<0>(t);
    }
    */

    /** Output operator for generic tuple. */
    template <typename ...T>
    std::ostream& operator<<(std::ostream& os, const std::tuple<T...>& t)
    {
      print_tuple<sizeof...(T)-1>::print(os, t);
      return os;
    }

    template <typename ... T>
    std::istream& operator>>(std::istream& is, std::tuple<T...>& t)
    {
      // FIXME: to be implemented when/if really needed (not likely)
      return is;
    }

    template <int... s, typename... T>
    auto ref_tuple_impl(tuple_index<s...> seq, std::tuple<T...>& tup)
      -> decltype(std::make_tuple(std::ref(std::get<s>(tup))...))
    {
      return std::make_tuple(std::ref(std::get<s>(tup))...);
    }

    template <typename... T>
    auto to_refs(std::tuple<T...>& tup)
      -> decltype(ref_tuple_impl(typename make_index<sizeof...(T)>::type(), tup))
    {
      std::tuple<std::reference_wrapper<T>...> rt = ref_tuple_impl(typename make_index<sizeof...(T)>::type(), tup);
      return rt;
    }


    /*
    template <class H, class S, class ...T>
    std::tuple<std::reference_wrapper<H>, std::reference_wrapper<S>, std::reference_wrapper<T>...> to_refs(const std::tuple<H, S, T...>& original)
    {
    std::reference_wrapper<H> head_ref = std::ref(const_cast<H&>(std::get<0>(original)));
    std::tuple<std::reference_wrapper<S>, std::reference_wrapper<T>...> tail_refs = to_refs(tuple_tail(original));
    return tuple_cat(make_tuple(head_ref), tail_refs);
    }

    template <class H>
    std::tuple<std::reference_wrapper<H>> to_refs(const std::tuple<H>& original)
    {
    std::reference_wrapper<H> head_ref = std::ref(const_cast<H&>(std::get<0>(original)));
    return make_tuple(head_ref);
    }
    */
  }
}

#endif
