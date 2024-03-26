//
//  main.cpp
//  poc-pointers
//
//  Created by Luca Di Gaspero on 03/03/23.
//

#include <utils.hh>
#include <concepts.hh>
#include <cost-components.hh>
#include <solution-manager.hh>
#include <neighborhood-explorer.hh>
#include "multi-modal-neighborhood-explorer.hh"
#include <plahc.hh>

#include <iostream>
#include <memory>
#include <tuple>
#include <algorithm>

#include <random>

class MyInput
{
public:
  MyInput(size_t x) noexcept : n(x) {}
  size_t n;
};

class MySolution
{
public:
  MySolution(std::shared_ptr<const MyInput> in) noexcept : in(in), v(in->n, 0) {}
  std::shared_ptr<const MyInput> in;
  std::vector<int> v;
};

// TODO: helper to transform a std::function (e.g., a lambda) in a CostComponent

class OneElements : public easylocal::CostComponent<MyInput, MySolution, int>
{
public:
  int ComputeCost(const MySolution& s) const 
  {
    int n = 0;
    for (auto& v : s.v)
      if (v == 1)
        n++;
    return n;
  }
};

class ZeroElements : public easylocal::CostComponent<MyInput, MySolution, int>
{
public:
  int ComputeCost(const MySolution& s) const override
  {
    int n = 0;
    for (auto& v : s.v)
      if (v == 0)
        n++;
    return n;
  }
};

class MySolutionManager : public easylocal::SolutionManager<MyInput, MySolution, int, easylocal::AggregatedCostStructure<MyInput, MySolution, int>>
{
public:
  std::shared_ptr<MySolution> InitialSolution(std::shared_ptr<MyInput> in) const
  {
    auto sol = std::make_shared<MySolution>(in);
    std::bernoulli_distribution dist(0.25);
    for (auto& v : sol->v)
      v = dist(rng);
    return sol;
  }
  
protected:
  mutable std::mt19937 rng;
};

class EvenSetOne
{
public:
  size_t index;
  int value;
};

class OddSetOne
{
public:
  size_t index;
  int value;
};

class EvenSetOneNeighborhoodExplorer : public easylocal::NeighborhoodExplorer<MySolutionManager, EvenSetOne, EvenSetOneNeighborhoodExplorer>
{
public:
  EvenSetOneNeighborhoodExplorer(std::shared_ptr<const MySolutionManager> sm) noexcept : easylocal::NeighborhoodExplorer<MySolutionManager, EvenSetOne, EvenSetOneNeighborhoodExplorer>(sm) {}

  easylocal::Generator<EvenSetOne> Neighborhood(std::shared_ptr<const MySolution> sol) const
  {
    for (size_t i = 0; i < sol->in->n; ++i)
    {
      for (int v = 0; v < 4; ++v)
        if (v % 2 == 0)
          co_yield EvenSetOne{i, v};
    }
  }

  EvenSetOne RandomMove(std::shared_ptr<const MySolution> sol) const
  {
    std::random_device dev;
    std::mt19937 rng(1234);
    std::uniform_int_distribution<std::mt19937::result_type> dist_index(0, sol->in->n), dist_value(0, 4);
    auto v = dist_index(rng), x = dist_value(rng);
    return EvenSetOne{v % 2 == 0 ? v : v + 1, int(x % 2 == 0 ? x : std::max<size_t>(x + 1, sol->in->n - 1))};
  }

  void MakeMove(std::shared_ptr<MySolution> sol, const EvenSetOne& mv) const
  {
    assert(mv.index < sol->v.size());
    sol->v[mv.index] = mv.value;
  }    
};

class OddSetOneNeighborhoodExplorer : public easylocal::NeighborhoodExplorer<MySolutionManager, OddSetOne, OddSetOneNeighborhoodExplorer>
{
public:
  OddSetOneNeighborhoodExplorer(std::shared_ptr<const MySolutionManager> sm) noexcept : easylocal::NeighborhoodExplorer<MySolutionManager, OddSetOne, OddSetOneNeighborhoodExplorer>(sm) {}

  easylocal::Generator<OddSetOne> Neighborhood(std::shared_ptr<const MySolution> sol) const
  {
    for (size_t i = 0; i < sol->in->n; ++i)
    {
      for (int v = 0; v < 4; ++v)
        if (v % 2 == 1)
          co_yield OddSetOne{i, v};
    }
  }

  OddSetOne RandomMove(std::shared_ptr<const MySolution> sol) const
  {
    std::random_device dev;
    std::mt19937 rng(1234);
    std::uniform_int_distribution<std::mt19937::result_type> dist_index(0, sol->in->n - 1), dist_value(0, 4);
    auto v = dist_index(rng), x = dist_value(rng);
    return OddSetOne{v % 2 == 1 ? v : v + 1, int(x % 2 == 1 ? x : std::max<size_t>(x + 1, sol->in->n - 1))};
  }

  void MakeMove(std::shared_ptr<MySolution> sol, const OddSetOne& mv) const
  {
    assert(mv.index < sol->v.size());
    sol->v[mv.index] = mv.value;
  }
};

class SetValue
{
public:
  size_t index;
  int value;
};

class DeltaZeroElements : public easylocal::DeltaCostComponent<MyInput, MySolution, int, SetValue>
{
public:
  int ComputeDeltaCost(const MySolution& s, const SetValue& mv) const override
  {
    if (s.v[mv.index] == 0 && mv.value != 0)
      return -1;
    else if (s.v[mv.index] != 0 && mv.value == 0)
      return 1;
    return 0;
  }
};

class SetValueNeighborhoodExplorer : public easylocal::NeighborhoodExplorer<MySolutionManager, SetValue, SetValueNeighborhoodExplorer>
{
public:
  SetValueNeighborhoodExplorer(std::shared_ptr<const MySolutionManager> sm) noexcept : easylocal::NeighborhoodExplorer<MySolutionManager, SetValue, SetValueNeighborhoodExplorer>(sm), rng(1234) {}

  easylocal::Generator<SetValue> Neighborhood(std::shared_ptr<const MySolution> sol) const
  {
    for (size_t i = 0; i < sol->in->n; ++i)
    {
      co_yield SetValue{i, 0};
    }
  }

  SetValue RandomMove(std::shared_ptr<const MySolution> sol) const
  {
    std::uniform_int_distribution<std::mt19937::result_type> dist_index(0, sol->in->n - 1), dist_value(0, 4);
    auto i = dist_index(rng);
    return SetValue{i, int(dist_value(rng))};
  }

  void MakeMove(std::shared_ptr<MySolution> sol, const SetValue& mv) const
  {
    assert(mv.index < sol->v.size());
    sol->v[mv.index] = mv.value;
  }
    
  mutable std::mt19937 rng;
};

std::ostream& operator<<(std::ostream& os, const MySolution& sol)
{
  os << "sol: ";
  std::copy(sol.v.begin(), sol.v.end(), std::ostream_iterator<int>(os, " "));
  return os;
}

std::ostream& operator<<(std::ostream& os, const EvenSetOne& so)
{
  os << "e: " << so.index << "/" << so.value;
  return os;
}

std::ostream& operator<<(std::ostream& os, const OddSetOne& so)
{
  os << "o: " << so.index << "/" << so.value;
  return os;
}

class UnionEvenOddNeighborhoodExplorer : public easylocal::UnionNeighborhoodExplorer<MySolutionManager, UnionEvenOddNeighborhoodExplorer, SetValueNeighborhoodExplorer, EvenSetOneNeighborhoodExplorer, OddSetOneNeighborhoodExplorer> {
    using easylocal::UnionNeighborhoodExplorer<MySolutionManager, UnionEvenOddNeighborhoodExplorer, SetValueNeighborhoodExplorer, EvenSetOneNeighborhoodExplorer, OddSetOneNeighborhoodExplorer>::UnionNeighborhoodExplorer;
};

int main(int argc, const char * argv[])
{
  std::shared_ptr<MyInput> p_in = std::make_shared<MyInput>(10);
  std::shared_ptr<MySolution> sol1 = std::make_shared<MySolution>(p_in), sol = std::make_shared<MySolution>(p_in);
//  sol1->v[0] = 1;
//  sol1->v[1] = 1;
  /* Solution sol2 = sol1;
  Solution sol3(p_in2);
  Solution sol4;
  std::cout << sol1 << sol2 << sol3 << sol4 << std::endl;
  sol4 = sol3;
  sol3 = sol2;
  std::cout << "DOPO" << std::endl;
  std::cout << sol1 << sol2 << sol3 << sol4 << std::endl; */
  auto oe = std::make_shared<OneElements>();
  auto ze = std::make_shared<ZeroElements>();
  
  std::shared_ptr<MySolutionManager> sm = std::make_shared<MySolutionManager>();
  sm->AddCostComponent(oe, 1);
  sm->AddCostComponent(ze, 1);
  
    std::shared_ptr<SetValueNeighborhoodExplorer> s_ne = std::make_shared<SetValueNeighborhoodExplorer>(sm);
    DeltaZeroElements dze;
    s_ne->AddDeltaCostComponent(dze, 1);
  
//  auto plahc = easylocal::PLAHC<MySolutionManager, SetValueNeighborhoodExplorer>(sm, s_ne, 10);
//  plahc.Run(p_in);
  // FIXME: it will be removed later

//  sm1.AddCostComponent(nze, false);
//  
//  auto c1 = sm.cost(sol1);
////  auto c2 = sm1.cost(sol1);
//  std::cout << c1.get(0) << " " << typeid(c1).name() << std::endl;
//  std::cout << c2.get(0) << " " << typeid(c2).name() << std::endl;
    EvenSetOneNeighborhoodExplorer e_ne(sm);
    OddSetOneNeighborhoodExplorer o_ne(sm);
//
//  for (auto mv : e_ne.neighborhood(sol1))
//    std::cout << mv << " ";
//  std::cout << std::endl;
//  auto mv = e_ne.random(sol1);
//  std::cout << "Random: " << mv << " " << e_ne.compute_delta(sol1, mv.value()).get(0) << std::endl;
//
    UnionEvenOddNeighborhoodExplorer u_ne(sm);

    u_ne.AddDeltaCostComponent<SetValue>(dze, 1);
    
    auto mv = u_ne.RandomMove(sol);
    
    auto has_it = u_ne.HasDeltaCostComponent(1, mv);
    if (has_it)
        u_ne.ComputeDeltaCost(sol, mv, 1);
    
    u_ne.InverseMove(sol, mv, mv);

//
////  auto mv2 = u_ne.random(sol1);
////  if (mv2.index() == 0)
////    std::cout << "Random 0 mnbh: " << std::get<0>(mv2) << std::endl;
////  else
////    std::cout << "Random 0 mnbh: " << std::get<1>(mv2) << std::endl;
//
////  auto t = std::make_tuple(e_ne, o_ne);
////  auto result = u_ne.random(sol1);
//
//  for (auto result : u_ne.neighborhood(sol1))
//  {
//    //  auto result = std::apply([&sol1](auto ...t) { return std::make_tuple(t...); }, t);
//    if (result.index() == 0)
//      std::cout << std::get<0>(result) << " ";
//    else
//      std::cout << std::get<1>(result) << " ";
//    u_ne.make_move(sol1, result);
//    std::copy(sol1->v.begin(), sol1->v.end(), std::ostream_iterator<int>(std::cout, " "));
//    std::cout << std::endl;
//  }
  
  // FIXME: shared vs object vs class vs whatever
//  std::shared_ptr<DeltaZeroElements> dze = std::make_shared<DeltaZeroElements>();
//  o_ne.AddDeltaCostComponent(dze, 1);
//  
//  std::cout << "----------" << std::endl;
//  std::copy(sol->v.begin(), sol->v.end(), std::ostream_iterator<int>(std::cout, " "));
//  std::cout << std::endl;
//  for (auto mv : o_ne.neighborhood(sol))
//  {
//    auto delta = o_ne.compute_delta(sol, mv);
//    std::cout << mv << " "  << delta.get(0) << "/" << delta.get(1) << std::endl;
//  }
//  
//  std::cout << "sol: ";
//  std::copy(sol->v.begin(), sol->v.end(), std::ostream_iterator<int>(std::cout, " "));
//  std::cout << std::endl;
//  std::cout << "sol1: ";
//  std::copy(sol1->v.begin(), sol1->v.end(), std::ostream_iterator<int>(std::cout, " "));
//  std::cout << std::endl;
//  std::cout << std::boolalpha << (sm.cost(sol1) < sm.cost(sol)) << std::endl;
  
  return 0;
}
