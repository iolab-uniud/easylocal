#if !defined(_TUPLE_HH_)
#define _TUPLE_HH_


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
template <typename H, typename S, typename ...T>
void print_tuple(std::ostream& os, const std::tuple<H, S, T...>& t)
{
  os << std::get<0>(t) << " ";
  auto temp_tuple_tail = tuple_tail(t);
  print_tuple<S, T...>(os, temp_tuple_tail);
}

/** Prints a generic tuple, base case. */
template <typename H>
void print_tuple(std::ostream& os, const std::tuple<H>& t)
{
  os << std::get<0>(t);
}

/** Output operator for generic tuple. */
template <typename ...T>
std::ostream& operator<<(std::ostream& os, const std::tuple<T...>& t)
{
  print_tuple<T...>(os, t);
  return os;
}

template <typename ... T>
std::istream& operator>>(std::istream& is, std::tuple<T...>& t)
{
  // FIXME: to be implemented when/if really needed (not likely)
  return is;
}


#endif