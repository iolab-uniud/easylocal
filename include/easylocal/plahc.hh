//
//  lahc.hh
//  poc
//
//  Created by Luca Di Gaspero on 13/03/23.
//

#pragma once

#include "solution-manager.hh"
#include "runner.hh"
#include <iostream>
#include <iterator>
#include <memory>
#include <spdlog/spdlog.h>

namespace easylocal {
  template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer>
  class PLAHC : public Runner<SolutionManager, NeighborhoodExplorer>
  {
  public:
    using Input = typename Runner<SolutionManager, NeighborhoodExplorer>::Input;
    using Solution = typename Runner<SolutionManager, NeighborhoodExplorer>::Solution;
    using T = typename Runner<SolutionManager, NeighborhoodExplorer>::T;
    using CostStructure = typename Runner<SolutionManager, NeighborhoodExplorer>::CostStructure ;
    using Move = typename Runner<SolutionManager, NeighborhoodExplorer>::Move;
    
    PLAHC(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t history_length) : Runner<SolutionManager, NeighborhoodExplorer>(sm, ne), history_length(history_length) {}  
  protected:

    virtual void Go(std::shared_ptr<const Input> in) override
    {
      size_t iteration = 0, idle_iteration = 0;
      this->ResetStopRun();
      std::vector<SolutionValue<Input, Solution, T, CostStructure>> history;
      history.reserve(history_length);
      for (size_t i = 0; i < history_length; ++i)
        history.push_back(this->sm->CreateSolutionValue(this->sm->InitialSolution(in)));
      iteration = 0;
      idle_iteration = 0;
      size_t index = 0;
      auto current_solution_value = history[0];
      while ((iteration < max_iterations || idle_iteration <= 0.02 * iteration) && !this->StopRun())
      {
        size_t next_index = (index + 1) % history.size();
        auto current_move_value = this->ne->CreateMoveValue(current_solution_value, this->ne->RandomMove(current_solution_value.GetSolution()));
        if (current_move_value < current_solution_value)
        {
          history[index] = current_move_value;
          current_solution_value = history[next_index];
          index = (index + 1) % history.size();
          idle_iteration = 0;
        }
        else if (current_move_value < history[next_index])
        {
          current_solution_value = history[next_index];
          history[next_index] = current_move_value;
          index = (index + 2) % history.size();
          idle_iteration = 0;
        } else {
          current_solution_value = history[next_index];
          index = (index + 1) % history.size();
          idle_iteration++;
        }
        iteration++;
      }
      // post process solutions to get the pareto set
      std::vector<SolutionValue<Input, Solution, T, CostStructure>> pareto_front;
      for (size_t i = 0; i < history.size(); ++i)
      {
        bool non_dominated = true, drop_equal_solutions = false;
        for (size_t j = 0; j < history.size(); ++j)
        {
          if (i == j)
            continue;
          if (history[i] > history[j])
          {
            non_dominated = false;
            break;
          }
          if constexpr(std::equality_comparable<Solution>)
          {
            if (*history[i].GetSolution() == *history[j].GetSolution() && i > j)
            {
              drop_equal_solutions = true;
              break;
            }
          }
        }
        if (non_dominated && !drop_equal_solutions)
          pareto_front.emplace_back(history[i]);
      }
      spdlog::info("Pareto front size: {}", pareto_front.size());
      for (const auto& sol : pareto_front)
      {
        auto values = sol.GetValues();
//        std::copy(values.begin(), values.end(), std::ostream_iterator<T>(std::cout, " "));
//        std::cout << std::endl;
        std::ostringstream oss;
        oss << (*(sol.GetSolution()));
        spdlog::info("{} ---> ({})", oss.str(), spdlog::fmt_lib::join(values, ", "));

        assert(sol.CheckValues());
      }
      spdlog::info("Iterations: {}", iteration);
    }
  protected:
    // parameters
    size_t max_iterations = 1000000;
    size_t history_length;
  };
}
