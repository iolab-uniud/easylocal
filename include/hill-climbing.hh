//
//  hill-climbing.hh
//  pfsp-ls
//
//  Created by Luca Di Gaspero on 24/07/23.
//

#pragma once

#include "solution-manager.hh"
#include "components.hh"
#include "runner.hh"
#include <iostream>
#include <sstream>
#include <thread>
#include <future>
#include <chrono>
#include <iterator>
#include <memory>
#include "spdlog/spdlog.h"

namespace easylocal {

//template <class Runner>
//concept RunnerIdleIterT = has_basic_typedefs<Runner> &&
//requires(Runner) {
//    { Runner::idle_iteration } -> std::same_as<size_t&>;
//};


//template <RunnerIdleIterT Runner>
//class IdleIterationsTermination
//{
//public:
//    IdleIterationsTermination(size_t max_idle_iterations) : max_idle_iterations(max_idle_iterations) {}
//    
//    virtual bool operator()(Runner* r)
//    {
//        spdlog::debug("HERE in operator()");
//        return r->idle_iteration > max_idle_iterations;
//    }
//protected:
//    size_t max_idle_iterations;
//};

//template <RunnerIdleIterT Runner>
//class TotalIterationsTermination
//{
//public:
//    TotalIterationsTermination(size_t max_iterations) : max_iterations(max_iterations) {}
//    
//    virtual bool operator()(Runner* r)
//    {
//        spdlog::debug("HERE in operator() of TIT");
//        return r->iteration > max_iterations;
//    }
//protected:
//    size_t max_iterations;
//};

template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer, template <class R> class TerminationCriterion = IdleIterationsTermination, template <class R> class SelectMove = SelectMoveRandom, template <class R> class AcceptMove = AcceptMoveImproveOrEqual>
class HillClimbing : public Runner<SolutionManager, NeighborhoodExplorer>
{
public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure;
    using Move = typename NeighborhoodExplorer::Move;
    using SelfClass = HillClimbing<SolutionManager, NeighborhoodExplorer, TerminationCriterion, SelectMove, AcceptMove>;
    
    using SolutionValue = typename SolutionManager::SolutionValue;
    using MoveValue = typename NeighborhoodExplorer::MoveValue;
    
    HillClimbing(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t random_seed) : Runner<SolutionManager, NeighborhoodExplorer>(sm, ne), random_seed(random_seed) {}
    
    void SetParameters(po::variables_map& vm, std::vector<std::string> to_pass_further) override
    {
        po::options_description desc("Set of parameters associated with the required HC.");
        termination.add_parameter(desc);
        select_move.add_parameter(desc);
        accept_move.add_parameter(desc);
        po::store(po::command_line_parser(to_pass_further).options(desc).run(), vm);
        po::notify(vm);
    }
    
protected:

    virtual void Go(std::shared_ptr<const Input> in) override
    {
        rng.seed(random_seed);
        PrintParameters();
        current_solution_value = std::make_shared<SolutionValue>(this->sm->CreateSolutionValue(this->sm->InitialSolution(in)));
        
        while (!termination.terminate(this) && !this->StopRun())
        {
            try
            {
                current_move_value = std::make_shared<MoveValue>(select_move.select(this));
            }
            catch (EmptyNeighborhood)
            {
#if !defined(NDEBUG)
                spdlog::debug("empty neighborhood encountered while exploring");
#endif
                break;
            }
            
            if (accept_move.accept(this))
            {
                // make move
                *current_solution_value = *current_move_value;
                idle_iteration = 0;
                std::ostringstream oss;
                oss << (*(current_solution_value->GetSolution()));
                // std::cout << oss.str() << std::endl;
                spdlog::info("{} --> {}", oss.str(), current_solution_value->AggregatedCost());
            }
            else
            {
                idle_iteration = idle_iteration + 1;
            }
            iteration = iteration + 1;
        }
        
        // post processings
        /*spdlog::info("Post processing after {} iterations (idle: {})", iteration, idle_iteration);
        auto values = current_solution_value->GetValues();
        std::ostringstream oss;
        oss << (*(current_solution_value->GetSolution()));
        spdlog::info("{} ---> ({})", oss.str(), spdlog::fmt_lib::join(values, ", "));
*/
        assert(current_solution_value->CheckValues());
        
        this->final_solution_value = std::make_shared<SolutionValue>(*current_solution_value);
    }
    
public:
    // object data
    size_t iteration = 0, idle_iteration = 0;
    std::shared_ptr<SolutionValue> current_solution_value;
    std::shared_ptr<MoveValue> current_move_value;    
protected:
    void PrintParameters()
    {
        termination.print_parameters();
    }
    // parametrized criteria
    TerminationCriterion<SelfClass> termination;
    SelectMove<SelfClass> select_move;
    AcceptMove<SelfClass> accept_move;
    mutable std::mt19937_64 rng;
    size_t random_seed;
};
}
