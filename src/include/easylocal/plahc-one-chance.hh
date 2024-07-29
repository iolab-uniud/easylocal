//
//  lahc.hh
//  poc
//
//  Created by Luca Di Gaspero on 13/03/23.
//

#pragma once

#include "solution-manager.hh"
#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <iterator>
#include <memory>

namespace easylocal {
  template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer>
  class PLAHC_ONE_CHANCE
  {
  public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure;
    using Move = typename NeighborhoodExplorer::Move;
    
    PLAHC_ONE_CHANCE(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t history_length) : sm(sm), ne(ne), history_length(history_length) {}
  
    void Run(std::shared_ptr<const Input> in, std::chrono::milliseconds timeout)
    {
      std::packaged_task<void(std::shared_ptr<const Input> in)> running_task([this](std::shared_ptr<const Input> in) {
        this->Run(in);
      });
      auto future = running_task.get_future();
      std::thread thr(std::move(running_task), in);
      future.wait_for(timeout);
      stop_run = true;
      thr.join();
    }

    void Run(std::shared_ptr<const Input> in)
    {
      stop_run = false;
      std::vector<SolutionValue<Input, Solution, T, CostStructure>> history;
      history.reserve(history_length);
      for (size_t i = 0; i < history_length; ++i)
        history.push_back(sm->CreateSolutionValue(sm->InitialSolution(in)));
      iteration = 0;
      idle_iteration = 0;
      size_t index = 0;
      auto current_solution_value = history[0];
      while ((iteration < max_iterations || idle_iteration <= 0.02 * iteration) && !stop_run)
      {
        size_t next_index = (index + 1) % history.size();
        auto current_move_value = ne->CreateMoveValue(current_solution_value, ne->RandomMove(current_solution_value.GetSolution()));
        if (current_move_value < current_solution_value)
        {
          history[index] = current_move_value;
          current_solution_value = history[next_index];
          index = (index + 1) % history.size();
          idle_iteration = 0;
        }
        // else if (current_move_value < history[next_index])
        // {
        //   current_solution_value = history[next_index];
        //   history[next_index] = current_move_value;
        //   index = (index + 2) % history.size();
        //   idle_iteration = 0;
        // }
        else {
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
      std::cout << "Pareto front size: " << pareto_front.size() << std::endl;
      for (const auto& sol : pareto_front)
      {
        std::cout << *(sol.GetSolution()) << " ---> ";
        auto values = sol.GetValues();
        std::copy(values.begin(), values.end(), std::ostream_iterator<T>(std::cout, " "));
        std::cout << std::endl;
      }
      std::cout << "Iterations: " << iteration << std::endl;
    }
  protected:
    std::shared_ptr<const SolutionManager> sm;
    std::shared_ptr<const NeighborhoodExplorer> ne;
    size_t iteration = 0, idle_iteration = 0, max_iterations = 1000000;
    // parameter
    size_t history_length;
    std::atomic_bool stop_run;
  };
}
