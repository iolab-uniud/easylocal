//
//  concepts.hh
//  easy-poc
//
//  Created by Luca Di Gaspero on 09/03/23.
//

// TODO: split concepts in atomic ones, consider writing your struct for template specialization (i.e., a struct with a constexpr auto value = true / false) depending on the template parameters
// template <class T>
// struct OddDetectorImpl {
//   static constexpr auto value = false;
// };

// template <template <class...> class Tmpl, class... Args>
// requires (sizeof...(Args) % 2 == 1)
// struct OddDetectorImpl<Tmpl<Args>> {
//   static constexpr auto value = true;
// };
// template class<T>
// concept OddDetector = OddDetectorImpl<T>::value;

// TODO: use concepts with static_assert(Concept<Class>) for proper testing the framework

#pragma once

#include <concepts>
#include <type_traits>
#include <string>
#include <iostream>
#include "utils.hh"

namespace easylocal {

  template <typename T>
  concept Number = std::is_arithmetic_v<T>;

  template <typename Input>
  concept InputT = std::is_constructible_v<Input, std::string> || std::is_constructible_v<Input, std::istream> || std::is_constructible_v<Input, int>; 

  template <typename Solution, typename Input>
  concept SolutionT = InputT<Input> && requires(Solution s) {
    //check that the solution class has a shared pointer to a const input object
  { s.in } -> std::same_as<std::shared_ptr<const Input>&>;
  };

  template <typename T>
  concept Printable = requires(std::ostream& os, const T& s) {
    // check that the class has a << operator for output
    { os << s };
  };

  template <typename Class, typename Input, typename Solution, typename T>
  concept match_basic_classes = InputT<Input> && 
    SolutionT<Solution, Input> && 
    Number<T>;

  template <typename Class>
  concept has_basic_typedefs = requires() {
    typename Class::Input;
    InputT<typename Class::Input>;
    typename Class::Solution;
    SolutionT<typename Class::Solution, typename Class::Input>;
    typename Class::T;
    Number<typename Class::T>;
  };

  template <typename Class, typename Input, typename Solution, typename T>
  concept match_basic_typedefs = match_basic_classes<Class, Input, Solution, T> && 
  requires() {
    typename Class::Input;
    typename Class::Solution;
    typename Class::T;
    std::same_as<typename Class::Input, Input>;
    std::same_as<typename Class::Solution, Solution>;
    std::same_as<typename Class::T, T>;
  };

  template <typename CostComponent, class Input, class Solution, typename T>
  concept CostComponentT = match_basic_classes<CostComponent, Input, Solution, T> && 
  requires(CostComponent cc, std::shared_ptr<const Solution> sol) {
    { cc.ComputeCost(sol) } -> std::same_as<T>;
  };

  template <typename DeltaCostComponent, class Input, class Solution, typename T, typename Move>
  concept DeltaCostComponentT = match_basic_classes<DeltaCostComponent, Input, Solution, T> && 
  requires(DeltaCostComponent dcc, std::shared_ptr<const Solution> sol, const Move& mv) {
    { dcc.ComputeDeltaCost(sol, mv) } -> std::same_as<T>;
      // TODO: check if still needed
//    { dcc.Components() } -> std::same_as<size_t>;
  };

  template <typename CostStructure, typename Input, typename Solution, typename T>
  concept CostStructureT = match_basic_typedefs<CostStructure, Input, Solution, T> && 
  requires(CostStructure s, std::shared_ptr<const Solution> sol, size_t i)
  {
    { s.ComputeCost(sol, i) } -> std::same_as<T>;
    { s.Components() } -> std::same_as<size_t>;
  };

  template <typename CostStructure>
  concept CostStructureTd = has_basic_typedefs<CostStructure> && 
  requires(CostStructure s, std::shared_ptr<const typename CostStructure::Solution> sol, size_t i)
  {
    { s.ComputeCost(sol, i) } -> std::same_as<typename CostStructure::T>;
    { s.Components() } -> std::same_as<size_t>;
  };

  template <class SolutionValue, class Input, class Solution, class T, class CostStructure>
  concept SolutionValueT = match_basic_typedefs<SolutionValue, Input, Solution, T> && CostStructureT<CostStructure, Input, Solution, T> && 
  requires(size_t i, SolutionValue sv) {
    { sv[i] } -> std::same_as<T>;
  };

  template<typename T, typename U>
  concept same_as_unqualified = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

  template <class SolutionManager>
  concept SolutionManagerT = has_basic_typedefs<SolutionManager> && 
  requires(std::shared_ptr<typename SolutionManager::Input> in, SolutionManager sm) {
    { sm.InitialSolution(in) } -> std::same_as<std::shared_ptr<typename SolutionManager::Solution>>;
    typename SolutionManager::CostStructure;
  };

  template <class NeighborhoodExplorer>
  concept NeighborhoodExplorerT = has_basic_typedefs<NeighborhoodExplorer> && 
requires(NeighborhoodExplorer ne, typename NeighborhoodExplorer::Solution& sol, std::shared_ptr<const typename NeighborhoodExplorer::Solution> cp_sol, std::shared_ptr<typename NeighborhoodExplorer::Solution> p_sol, typename NeighborhoodExplorer::Move mv) {
    typename NeighborhoodExplorer::SolutionManager;
    std::is_constructible_v<std::shared_ptr<const typename NeighborhoodExplorer::SolutionManager>>;
    typename NeighborhoodExplorer::Move;
    { ne.RandomMove(cp_sol) } -> std::same_as<typename NeighborhoodExplorer::Move>;
    { ne.MakeMove(p_sol, mv)};
    { ne.Neighborhood(cp_sol)} -> std::same_as<Generator<typename NeighborhoodExplorer::Move>>;
  };

  template <class NeighborhoodExplorer>
  concept has_inverse_move = 
requires(NeighborhoodExplorer ne, typename NeighborhoodExplorer::Solution& sol, typename NeighborhoodExplorer::Move mv1, typename NeighborhoodExplorer::Move mv2) {
    { ne.InverseMove(sol, mv1, mv2) } -> std::same_as<bool>;
  };
}
