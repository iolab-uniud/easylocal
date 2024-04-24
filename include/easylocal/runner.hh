//
//  runner.hh
//  pfsp-ls
//
//  Created by Luca Di Gaspero on 24/07/23.
//

#pragma once

#include <thread>
#include <future>
#include <chrono>
#include <atomic>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace easylocal {

template <SolutionManagerT SolutionManager>
class AbstractRunner
{
public:
    using Input = typename SolutionManager::Input;
    using SolutionValue = typename SolutionManager::SolutionValue;
    
    virtual SolutionValue Run(std::shared_ptr<const Input> in, std::chrono::milliseconds timeout) = 0;
    virtual void SetParameters(po::variables_map& /* vm */, std::vector<std::string> /* to_pass_further */) {};
};


template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer>
class Runner : public AbstractRunner<SolutionManager>
{
public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure ;
    using Move = typename NeighborhoodExplorer::Move;
    using SolutionValue = typename SolutionManager::SolutionValue;
    
    
protected:
    Runner(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne) : sm(sm), ne(ne) {}
    // virtual void SetParameters(po::variables_map& vm, std::vector<std::string> to_pass_further){};
    
public:
    
    SolutionValue Run(std::shared_ptr<const Input> in, std::chrono::milliseconds timeout) override
    {
        std::packaged_task<void(std::shared_ptr<const Input> in)> running_task([this](std::shared_ptr<const Input> in) {
            this->ResetStopRun();
            this->Go(in);
        });
        auto future = running_task.get_future();
        std::thread thr(std::move(running_task), in);
        future.wait_for(timeout);
        stop_run = true;
        thr.join();
        return *(final_solution_value);
    }
    
    inline void Run(std::shared_ptr<const Input> in)
    {
        this->Go(in);
    }
    
protected:
    
    virtual void Go(std::shared_ptr<const Input> in) = 0;
    
    inline void ResetStopRun()
    {
        stop_run = false;
    }
    
    inline bool StopRun() const
    {
        return stop_run;
    }
    
public:
    std::shared_ptr<const SolutionManager> sm;
    std::shared_ptr<const NeighborhoodExplorer> ne;
    std::atomic_bool stop_run;
    std::shared_ptr<SolutionValue> final_solution_value;
};
}

