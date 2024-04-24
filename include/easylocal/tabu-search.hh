//
//  tabu-search.hh
//
//  Created by Luca Di Gaspero on 24/07/23.
//

#pragma once

#include "solution-manager.hh"
#include "components.hh"
#include "runner.hh"
#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <iterator>
#include <memory>
#include "spdlog/spdlog.h"
#include <functional>
#include <boost/program_options.hpp>

namespace po = boost::program_options;


namespace easylocal {

// TODO: review all the names of templates / classes

template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer, template <class R> class TerminationCriterion, template <class R> class TabuList, template <class R> class AspirationCriterion, template <class R> class StopExplorationCriterion, template <class R> class NeighborhoodGenerator>
class TabuSearch : public Runner<SolutionManager, NeighborhoodExplorer>
{
public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure;
    using Move = typename NeighborhoodExplorer::Move;
    using SelfClass = TabuSearch<SolutionManager, NeighborhoodExplorer, TerminationCriterion, TabuList, AspirationCriterion, StopExplorationCriterion, NeighborhoodGenerator>;
    
    using SolutionValue = typename SolutionManager::SolutionValue;
    using MoveValue = typename NeighborhoodExplorer::MoveValue;
    
    TabuSearch(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t random_seed) : Runner<SolutionManager, NeighborhoodExplorer>(sm, ne), random_seed(random_seed) {}
    
    void SetParameters(po::variables_map& vm, std::vector<std::string> to_pass_further) override
    {
        po::options_description desc("Set of parameters associated with the required TS.");
        termination.add_parameter(desc);
        tabu_list.add_parameter(desc);
        aspiration.add_parameter(desc);
        stop_exploration.add_parameter(desc);
        neighborhood_generator.add_parameter(desc);
        po::store(po::command_line_parser(to_pass_further).options(desc).run(), vm);
        po::notify(vm);
    }
    
protected:

    virtual void Go(std::shared_ptr<const Input> in) override
    {
        tabu_list.initialize(this);
        PrintParameters();
        current_solution_value = std::make_shared<SolutionValue>(this->sm->CreateSolutionValue(this->sm->InitialSolution(in)));
        best_solution_value = std::make_shared<SolutionValue>(*current_solution_value);
        // TODO: 1. implement aspiration plus and elite candidate plus strategies
        while (!termination.terminate(this) && !this->StopRun()) //TODO: StopRun qui? da altre parti?
        {
            bool best_move_value_initialized = false;
            stop_exploration.initialize(this);
            try
            {
                for (auto _cmv : neighborhood_generator.generate_moves(this))
                {
                    current_move_value = _cmv;
                    if (tabu_list.is_tabu(this) && !aspiration.is_tabu_status_overridden(this))
                    {
                        continue;
                    }
                    if (!best_move_value_initialized || *current_move_value < *best_move_value)
                    {
                        best_move_value = std::make_shared<MoveValue>(*current_move_value);
                        best_move_value_initialized = true;
                    }
                    stop_exploration.update(this);
                    if (stop_exploration.has_to_stop(this))
                    {
                        break;
                    }
                }
            }
            catch (EmptyNeighborhood)
            {
#if !defined(NDEBUG)
                spdlog::debug("empty neighborhood encountered while exploring");
#endif
                break;
            }
            
            if (!best_move_value_initialized)
            {
                // spdlog::warn("HERE");
                if (!aspiration.use_least_tabu(this))
                {
#if !defined(NDEBUG)
                    spdlog::debug("best_move_value_initialized not initialized and you are not using least_tabu_move");
#endif
                    break;
                }
                else
                {
                    best_move_value = std::make_shared<MoveValue>(this->ne->CreateMoveValue(*current_solution_value, tabu_list.least_tabu(this)));
                }
            }
            
            current_solution_value = std::make_shared<SolutionValue>(*best_move_value);
            std::ostringstream oss;
            oss << (*(current_solution_value->GetSolution()));
            // std::cout << oss.str() << std::endl;
            spdlog::info("{} --> {}", oss.str(), current_solution_value->AggregatedCost());
            if (*current_solution_value < *best_solution_value)
            {
                best_solution_value = std::make_shared<SolutionValue>(*current_solution_value);
                idle_iteration = 0;
            }
            else
            {
                idle_iteration = idle_iteration + 1;
            }
            // update iterations
            iteration = iteration + 1;
            tabu_list.update(this);
            stop_exploration.update(this);
            // To remove later
#if !defined(NDEBUG)
            std::ostringstream oss;
            // oss << best_move_value->GetMove();
            //auto values = current_solution_value->GetValues();
            //spdlog::debug("TS - move selected {} / {} {} / {} ", iteration, idle_iteration, oss.str(), spdlog::fmt_lib::join(values, ", "));
#endif
        }
        // post processings
        // auto values = best_solution_value->GetValues();
        // std::ostringstream oss;
        // oss << (*(best_solution_value->GetSolution()));
        // spdlog::info("{} ---> ({})", oss.str(), spdlog::fmt_lib::join(values, ", "));
        // spdlog::info("Idle iterations: {} // Total iterations: {}", idle_iteration, iteration);
#if !defined(NDEBUG)
        spdlog::debug("Very end: iteration: {} // idle_iteration:{}", iteration, idle_iteration);
        spdlog::debug("Checking current solution");
        assert(current_solution_value->CheckValues());
        spdlog::debug("Checking best solution");
#endif
        assert(best_solution_value->CheckValues());
        this->final_solution_value = std::make_shared<SolutionValue>(*best_solution_value);
    }
    void PrintParameters()
    {
        // spdlog::info("Paremeter random_seed: {}", random_seed);
        termination.print_parameters();
        tabu_list.print_parameters();
        aspiration.print_parameters();
        stop_exploration.print_parameters();
        neighborhood_generator.print_parameters();
    }
public:
    // object data
    size_t iteration = 0, idle_iteration = 0;
    size_t metric_aspiration_used = 0;
    size_t random_seed;
    std::shared_ptr<SolutionValue> current_solution_value, best_solution_value;
    std::shared_ptr<MoveValue> current_move_value, best_move_value;
protected:
    // parametrized criteria
    TerminationCriterion<SelfClass> termination;
    TabuList<SelfClass> tabu_list;
    AspirationCriterion<SelfClass> aspiration;
    StopExplorationCriterion<SelfClass> stop_exploration;
    NeighborhoodGenerator<SelfClass> neighborhood_generator;
};

}
