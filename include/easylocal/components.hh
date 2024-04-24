#pragma once

#include "spdlog/spdlog.h"
#include <list>
#include <vector>
#include <map>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

// FIXME: -1 and size_t are archenemies!

// TODO: OR and AND of different components?
// TODO: divide components by type... (e.g., sub-directory with components and each type of component in a file)

namespace easylocal {

// TODO: currently it cannot check because idle_iteration is meant to be protected
template <class Runner>
concept RunnerIdleIterT = has_basic_typedefs<Runner>; /* &&
                                                       requires(Runner r) {
                                                       { r.idle_iteration } -> std::same_as<size_t&>;
                                                       }; */

class Parametrized
{
public:
    virtual void add_parameter(po::options_description& opt)
    {}
    virtual void print_parameters()
    {}
};


// TODO: give a more meaningful name
template <class Runner>
class FullNeighborhoodGenerator : public Parametrized
{
    using MoveValue = typename Runner::MoveValue;
public:
    easylocal::Generator<std::shared_ptr<MoveValue>> generate_moves(Runner* r)
    {
        for (auto mv : r->ne->Neighborhood(r->current_solution_value->GetSolution()))
        {
            co_yield std::make_shared<MoveValue>(r->ne->CreateMoveValue(*(r->current_solution_value), mv));
        }
    }
    virtual void initialize()
    {}
protected:
};

// TODO: give a more meaningful name
template <class Runner>
class EliteCandidateGenerator : public Parametrized
{
    using MoveValue = typename Runner::MoveValue;
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("k", po::value<size_t>(&k), "Size of the elite candidate list.");
        opt.add_options()
            ("theta", po::value<double>(&theta), "Size of the threshold (must be equal or greater than 1.0).");
    }
    void print_parameters() override
    {
        // spdlog::info("EliteCandidateGenerator - parameter k: {}", k);
        // spdlog::info("EliteCandidateGenerator - parameter theta: {}", theta);
    }
    void initialize()
    {
        assert(theta >= 1.0);
    }
    easylocal::Generator<std::shared_ptr<MoveValue>> generate_moves(Runner* r)
    {
        size_t best_index;
        threshold = (r->best_solution_value)->AggregatedCost() * theta;
        if (elite_candidates.size() == 0)
            build_elite_candidate_list(r);
        // if the candidate list is not empty
        assert(elite_candidates.size() > 0);
        best_index = search_best_elite_candidate_list(r);
        MoveValue best_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[best_index].GetMove());
        
        if (best_move_value.AggregatedCost() <= threshold)
        {
            co_yield std::make_shared<MoveValue>(best_move_value);
            elite_candidates.erase(elite_candidates.begin() + best_index);
        }
        else
        {
            build_elite_candidate_list(r);
            best_index = search_best_elite_candidate_list(r);
            MoveValue best_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[best_index].GetMove());
            if (best_move_value.AggregatedCost() > threshold)
                co_return;
            co_yield std::make_shared<MoveValue>(best_move_value);
            elite_candidates.erase(elite_candidates.begin() + best_index);
        }
    }
protected:
    void build_elite_candidate_list(Runner* r)
    {
        // clear the candidate list,
        elite_candidates.clear();
        // worse_move_index
        size_t worse_move_index = 0;
        // worse_move
        MoveValue worse_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), r->ne->RandomMove(r->current_solution_value->GetSolution()));
        bool initialized = false;
        // for mv in neighborhood
        for (auto mv : r->ne->Neighborhood(r->current_solution_value->GetSolution()))
        {
            MoveValue current_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), mv);
            // if candidate list size < k
            if (elite_candidates.size() < k)
            {
                if(!initialized)
                {
                    initialized = true;
                    worse_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), mv);
                }
                // pushback
                elite_candidates.push_back(current_move_value);
                if (worse_move_value.AggregatedCost() < current_move_value.AggregatedCost())
                {
                    worse_move_value = current_move_value;
                    worse_move_index = elite_candidates.size() - 1; // - 1 because I already updated the candidate list
                }
            }
            // else
            else
            {
                // if better than worse, then put this in that place
                if(current_move_value.AggregatedCost() < worse_move_value.AggregatedCost())
                {
                    elite_candidates[worse_move_index] = current_move_value;
                    // find the new worse
                    worse_move_value = elite_candidates[0];
                    worse_move_index = 0;
                    for(size_t i = 1; i < elite_candidates.size(); ++i)
                    {
                        if (worse_move_value.AggregatedCost() < elite_candidates[i].AggregatedCost())
                        {
                            worse_move_index = i;
                            worse_move_value = elite_candidates[i];
                        }
                    }
                }
            }
        }
    }
    
    size_t search_best_elite_candidate_list(Runner* r)
    {
        size_t best_index = 0;
        MoveValue best_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[0].GetMove());
        for (size_t i = 1; i < elite_candidates.size(); ++i)
        {
            MoveValue current_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[i].GetMove());
            if (current_move_value.AggregatedCost() < best_move_value.AggregatedCost())
            {
                best_move_value = current_move_value;
                best_index = i;
            }
        }
        return best_index;
    }
    size_t k;
    std::vector<MoveValue> elite_candidates;
    double theta;
    T threshold;
};

template <RunnerIdleIterT Runner>
class IdleIterationsTermination : public Parametrized
{
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-idle-iteration", po::value<size_t>(&max_idle_iterations), "Max number of idle iterations.");
    }
    void print_parameters() override
    {
        // spdlog::info("IdleIterationsTermination - parameter max_idle_iterations: {}", max_idle_iterations);
    }
    bool terminate(Runner* r)
    {
        return r->idle_iteration > max_idle_iterations;
    }
protected:
    size_t max_idle_iterations;
};

template <RunnerIdleIterT Runner>
class TotalIterationsTermination : public Parametrized
{
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-total-iterations", po::value<size_t>(&max_iterations), "Maximum number of iterations.");
    }
    void print_parameters() override
    {
        // spdlog::info("TotalIterationsTermination - parameter max_iterations: {}", max_iterations);
    }
    bool terminate(Runner* r)
    {
        return r->iteration > max_iterations;
    }
protected:
    size_t max_iterations;
};

// TODO: define the proper concept for TabuList
template <class Runner>
class FixedLengthTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length", po::value<size_t>(&max_length), "Maximum length of the tabu list.");
    }
    void print_parameters() override
    {
        // spdlog::info("FixedLengthTabuList - parameter max_length: {}", max_length);
        // spdlog::info("FixedLengthTabuList - parameter current: {}", current);
    }
    void initialize(Runner* r)
    {
        current = 0;
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                std::ostringstream oss;
                // oss << tl_move;
                //spdlog::debug("FixedLengthTabuList - move {} is tabu", oss.str());
#endif
                return true;
            }
        }
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthTabuList - move is NOT tabu");
#endif
        return false;
    }
    void update(Runner* r)
    {
        if (tabu_moves.size() < max_length)
        {
            tabu_moves.push_back(r->best_move_value->GetMove());
        }
        else
        {
            tabu_moves[current] = r->best_move_value->GetMove();
            current = (current + 1) % max_length;
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthTabuList - retrieve least tabu move");
#endif
        return tabu_moves[current];
    }
protected:
    std::vector<Move> tabu_moves;
    size_t current, max_length;
};

template <class Runner>
class FixedLengthObjectiveBasedTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length", po::value<size_t>(&max_length), "Maximum length of the tabu list.");
    }
    void print_parameters() override
    {
        // spdlog::info("FixedLengthObjectiveBasedTabuList - parameter max_length: {}", max_length);
        // spdlog::info("FixedLengthObjectiveBasedTabuList - parameter current: {}", current);
    }
    void initialize(Runner* r)
    {
        current = 0;
    }
    bool is_tabu(Runner* r)
    {
        const T& current_move = r->current_move_value->AggregatedCost();
        // const auto& current_solution = r->current_move_value->GetSolution();
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthObjectiveBasedTabuList - move cost is {}", current_move);
#endif
        for (const auto& tl_move : tabu_moves)
        {
            if (tl_move == current_move)
            {
#if !defined(NDEBUG)
                spdlog::debug("FixedLengthObjectiveBasedTabuList - move {} is tabu", tl_move);
#endif
                return true;
            }
        }
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthObjectiveBasedTabuList - move is NOT tabu");
#endif
        return false;
    }
    void update(Runner* r)
    {
        if (tabu_moves.size() < max_length)
        {
            tabu_moves.push_back(r->best_move_value->AggregatedCost());
        }
        else
        {
            tabu_moves[current] = r->best_move_value->AggregatedCost();
            current = (current + 1) % max_length;
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthObjectiveBasedTabuList - retrieve least tabu move");
#endif
        return r->ne->RandomMove(r->current_solution_value->GetSolution());
    }
protected:
    std::vector<T> tabu_moves;
    size_t current, max_length;
};

template <class Runner>
class LimDynamicTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length", po::value<size_t>(&max_length), "Maximum length of the tabu list.")
            ("min-length", po::value<size_t>(&min_length), "Minimum length of the tabu list.")
            ("iteration-threshold", po::value<size_t>(&iteration_threshold), "Threshold for iterations");
    }
    void print_parameters() override
    {
        // spdlog::info("LimDynamicTabuList - parameter max-length: {}", max_length);
        // spdlog::info("LimDynamicTabuList - parameter min-length: {}", min_length);
        // spdlog::info("LimDynamicTabuList - parameter - iteration_threshold: {}", iteration_threshold);
        // spdlog::info("LimDynamicTabuList - parameter current: {}", current);
    }
    void initialize(Runner* r)
    {
        current = 0;
        assert(min_length < max_length);
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        if (tabu_moves.size() < min_length || r->idle_iteration >= iteration_threshold)
        {
            tabu_moves.push_back(r->best_move_value->GetMove());
        }
        else
        {
            tabu_moves[current] = r->best_move_value->GetMove();
            current = (current + 1) % tabu_moves.size();
        }
        
        if (r->idle_iteration == 0 || tabu_moves.size() >= max_length)
        {
            size_t cnt;
            if (tabu_moves.size() > min_length)
                cnt = tabu_moves.size() - min_length;
            else
                cnt = min_length - tabu_moves.size();
            
            while (cnt > 0)
            {
                tabu_moves.erase(tabu_moves.begin() + current);
                current = (current + 1) % tabu_moves.size();
                cnt--;
            }
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("LimDynamicTabuList - retrieve least tabu move");
#endif
        return tabu_moves[current];
    }
protected:
    std::vector<Move> tabu_moves;
    size_t current, max_length, min_length, iteration_threshold;
};

template <class Runner>
class TaillardTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length-it", po::value<size_t>(&max_length_it), "Insert description")
            ("tabu-sizes", po::value<std::vector<size_t>>(&tabu_sizes), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("TaillardTabuList - parameter max_length_it: {}", max_length_it);
        // spdlog::info("TaillardTabuList - parameter tabu_sizes: {}", spdlog::fmt_lib::join(tabu_sizes, ", "));
        // spdlog::info("TaillardTabuList - parameter current: {}", current);
        // spdlog::info("TaillardTabuList - parameter current_length_index: {}", current_length_index);
    }
    void initialize(Runner* r)
    {
        current = 0;
        current_length_index = 0;
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        current_length = tabu_sizes[current_length_index];
        
        if (tabu_moves.size() < current_length)
        {
            tabu_moves.push_back(r->best_move_value->GetMove());
#if !defined(NDEBUG)
            spdlog::debug("TaillardTabuList - Adding move to the list, because list is to small ({}// {})", tabu_moves.size() - 1, current_length);
#endif
        }
        else
        {
            tabu_moves[current] = r->best_move_value->GetMove();
#if !defined(NDEBUG)
            spdlog::debug("TaillardTabuList - Changing index {} in tabu list of size {}", current, current_length);
#endif
            current = (current + 1) % current_length;
        }
        
        if (length_it == max_length_it)
        {
            current_length_index = (current_length_index + 1) % tabu_sizes.size();
            length_it = 0;
        }
        else
        {
            length_it = length_it + 1;
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("TaillardTabuList - retrieve least tabu move");
#endif
        size_t i = (current + 1) % current_length;
        return tabu_moves[i];
    }
protected:
    std::vector<Move> tabu_moves;
    std::vector<size_t> tabu_sizes;
    size_t current, current_length_index; // indexes
    size_t current_length; // this is the current size to consider of tabu_moves
    size_t length_it, max_length_it; // this is the number of iterations that the current_length has been used
};

template <class Runner>
class GendrauTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    //FIXME: rng 10, 10 should be something more principled as the seed for e.g.
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("min-iteration-tl", po::value<size_t>(&min_iteration), "Minimum number of iterations you can have.");
        opt.add_options()
            ("max-iteration-tl", po::value<size_t>(&max_iteration), "Maximum number of iterations you can have.");
    }
    void print_parameters() override
    {
        // spdlog::info("GendrauTabuList - parameter min_iteration: {}", min_iteration);
        // spdlog::info("GendrauTabuList - parameter max_iteration: {}", max_iteration);
        // // spdlog::info("GendrauTabuList - parameter random seed: {}", rng.seed());
    }
    void initialize(Runner* r)
    {
        rng.seed(r->random_seed);
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move.first))
            {
#if !defined(NDEBUG)
                spdlog::debug("GendrauTabuList - move is tabu");
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("GendrauTabuList - update tabu list");
#endif
        // insert in the tabu list best_move_value, the iteration will be current_iteration + number of iterations drown at random
        std::uniform_int_distribution<size_t> dist_iterations(min_iteration, max_iteration);
        size_t delta_iteration = dist_iterations(rng);
        size_t removal_iteration = delta_iteration + r->iteration;
        tabu_moves.push_back(std::make_pair(r->best_move_value->GetMove(), removal_iteration));
        // scan tabu_moves, if some moves have tl_move.second == current iteration, remove it
        for (size_t i = 0; i < tabu_moves.size(); ++i)
        {
            if (tabu_moves[i].second == r->iteration)
            {
                tabu_moves.erase(tabu_moves.begin() + i);
            }
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("GendrauTabuList - retrieve least tabu move");
#endif
        Move least_tabu = tabu_moves[0].first;
        size_t least_it = tabu_moves[0].second;
        for (size_t i = 1; i < tabu_moves.size(); ++i)
        {
            if (tabu_moves[i].second < least_it)
            {
                least_tabu = tabu_moves[i].first;
                least_it = tabu_moves[i].second;
            }
        }
        return least_tabu;
    }
protected:
    std::vector<std::pair<Move, size_t>> tabu_moves; // tabu_moves contain a pair, that is the move and the iteration at which the move will be removed
    size_t min_iteration, max_iteration;
    mutable std::mt19937_64 rng;
};

template <class Runner>
class ReactiveTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using Solution = typename Runner::Solution;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("cycle-max", po::value<size_t>(&cycle_max), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("ReactiveTabuList - parameter cycle_max: {}", cycle_max);
        // spdlog::info("ReactiveTabuList - parameter cycleMoveAve: {}", cycleMoveAve);
        // spdlog::info("ReactiveTabuList - parameter last_update_iteration: {}", last_update_iteration);
    }
    void initialize(Runner* r)
    {
        cycleMoveAve = 0;
        last_update_iteration = 0;
    }

    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                std::ostringstream oss;
                oss << tl_move;
                spdlog::debug("ReactiveTabuList - move {} is tabu", oss.str());
#endif
                return true;
            }
        }
#if !defined(NDEBUG)
        spdlog::debug("ReactiveTabuList - move is NOT tabu");
#endif
        return false;
    }
    void update(Runner* r)
    {
        // see: https://github.com/reichlin/Kitchen2000/blob/master/Reactive%20tabu%20Search/RTS.java#L397
        const auto& current_move = r->best_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        const size_t& current_iteration = r->iteration;
        
        // find whether the current solution was already encountered, and set last_met_iteration accordingly
        if (already_met_solution(current_solution, current_iteration))
        {
#if !defined(NDEBUG)
            spdlog::debug("ReactiveTabuList - solution met");
#endif
            // update cycleMoveAve
            cycleMoveAve = (size_t) ((cycleMoveAve * current_iteration + last_met_iteration) / current_iteration);
            // update lastupdate
            last_update_iteration = current_iteration;
        }
        // else if (iterations - last_update) > cycloMoveAve
        else if(current_iteration - last_update_iteration > cycleMoveAve)
        {
            // if tabu list is not empty
            if (tabu_moves.size() > 0)
                // remove the first element of the tabu list
                tabu_moves.erase(tabu_moves.begin());
        }
        // if history contain the maximum (cycle_max)
        if (history.size() >= cycle_max)
            // TODO: safely remove cycle_max - history.size() + 1 solutions
            // remove the first element of the tabu list
            history.erase(history.begin());
        
        // enqueue in tabu_moves and history the move and the current solution rispectively
        history.push_back(current_solution);
        tabu_moves.push_back(current_move);
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("ReactiveTabuList - retrieve least tabu move");
#endif
        // the least tabu move is the oldest one
        return tabu_moves[0];
    }
protected:
    bool already_met_solution(std::shared_ptr<const Solution> solution, size_t current_iteration)
    {
        if (history.size() == 0)
            return false;
        // look for it
        for (size_t i = history.size() - 1; i >= 0; --i)
        {
            if (history[i] == solution)
            {
                last_met_iteration = current_iteration + history.size() - i;
                return true;
            }
            
        }
        return false;
    }
    std::vector<Move> tabu_moves;
    std::vector<std::shared_ptr<const Solution>> history;
    size_t cycle_max, cycleMoveAve;
    size_t last_met_iteration, last_update_iteration;
};

template <class Runner>
class TransitionMeasureTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("frequency", po::value<double>(&frequency), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("TransitionMeasureTabuList - parameter frequency: {}", frequency);
    }
    void initialize(Runner* r)
    {}
    bool is_tabu(Runner* r)
    {
        // look in the transition measure table, if the hash of the move is there and it's frequency is above the given threshold, then return true, else it is false
        const auto& current_move = r->current_move_value->GetMove();
        size_t current_move_hash = r->ne->HashMove(current_move);
        if (transition_measure_table.contains(current_move_hash))
        {
            if (transition_measure_table[current_move_hash] / r->iteration >= frequency)
            {
#if !defined(NDEBUG)
                std::ostringstream oss;
                oss << current_move;
                spdlog::debug("TransitionMeasureTabuList - is tabu - {} Is tabu and the key in the hash table, with frequency {}", oss.str(), transition_measure_table[current_move_hash]);
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        // if the hash of the current move is present, then add 1, else it is equal to 1
        const auto& best_move = r->best_move_value->GetMove();
        size_t best_move_hash = r->ne->HashMove(best_move);
        if(transition_measure_table.contains(best_move_hash))
        {
            transition_measure_table[best_move_hash] =  transition_measure_table[best_move_hash] + 1;
        }
        else
        {
            transition_measure_table[best_move_hash] = 1;
        }
#if !defined(NDEBUG)
        for (auto p : transition_measure_table)
        {
            spdlog::debug("Update {} ---> {}", p.first, p.second);
        }
#endif
    }
    Move least_tabu(Runner* r)
    {
        // return the move that has the lower measure
        // thing is in transition_measure_table, I am saving possibly just part of the move, not the entire move, so how can I go back?
        return r->ne->RandomMove(r->current_solution_value->GetSolution());
    }
protected:
    std::map<size_t, size_t> transition_measure_table;
    double frequency;
};

template <class Runner>
class FooSchemeTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("phi", po::value<size_t>(&phi), "Insert description")
            ("ita", po::value<size_t>(&ita), "Insert description")
            ("bi", po::value<T>(&bi), "Insert description");

    }
    void print_parameters() override
    {
        // spdlog::info("FooSchemeTabuList - parameter phi: {}", phi);
        // spdlog::info("FooSchemeTabuList - parameter ita: {}", ita);
        // spdlog::info("FooSchemeTabuList - parameter bi: {}", bi);
        // spdlog::info("FooSchemeTabuList - parameter current: {}", current);
        // spdlog::info("FooSchemeTabuList - parameter last_it_update: {}", last_it_update);
        // spdlog::info("FooSchemeTabuList - parameter max_current_size: {}", max_current_size);
        // spdlog::info("FooSchemeTabuList - parameter min_initialized: {}", min_initialized);
        // spdlog::info("FooSchemeTabuList - parameter max_initialized: {}", max_initialized);
    }
    void initialize(Runner* r)
    {
        current = 0;
        last_it_update = 0;
        max_current_size = ita;
        min_initialized = false;
        max_initialized = false;
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                spdlog::debug("FooSchemeTabuList - move is tabu");
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        if(r->iteration == 0)
        {
            max_current_size = ita;
        }
        const Move& best_move = r->best_move_value->GetMove();
        const auto& best_move_cost = r->best_move_value->AggregatedCost();
        
        if (!min_initialized && !max_initialized)
        {
            min_o = best_move_cost;
            max_o = best_move_cost;
            min_initialized = true;
            max_initialized = true;
        }
        else
        {
            if (best_move_cost < min_o)
                min_o = best_move_cost;
            if(best_move_cost > max_o)
                max_o = best_move_cost;
            assert(min_o <= max_o);
        }
        
        if(r->iteration - last_it_update < phi)
        {
            add_or_update_current(r, best_move);
        }
        else
        {
            update_size(r, best_move);
        }
    }
    Move least_tabu(Runner* r)
    {
        return tabu_moves[current];
    }
protected:
    size_t current;
    size_t last_it_update;
    bool min_initialized, max_initialized;
    T min_o, max_o;
    std::vector<Move> tabu_moves;
    size_t max_current_size;
    // parameters
    size_t phi, ita;
    T bi;
    
    void add_or_update_current(Runner* r, Move best_move)
    {
#if !defined(NDEBUG)
        spdlog::debug("size is {} // current max size is {} // ita is ", tabu_moves.size(), max_current_size, ita);
#endif
        if (tabu_moves.size() < max_current_size)
        {
            tabu_moves.push_back(best_move);
            
        }
        else
        {
            tabu_moves[current] = best_move;
            current = (current + 1) % max_current_size;
        }
    }
    
    void update_size(Runner* r, Move best_move)
    {
        if (max_o - min_o < bi)
        {
            max_current_size = max_current_size + ita;
        }
        else
        {
            if (tabu_moves.size() > 0)
            {
                tabu_moves.erase(tabu_moves.begin() + current);
                max_current_size = max_current_size - 1;
                current = (current + 1) % max_current_size;
            }
        }
         max_initialized = false;
         min_initialized = false;
         last_it_update = r->iteration;
    }
};

template <class Runner>
class RandomFooSchemeTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-phi", po::value<size_t>(&max_phi), "Insert description")
            ("min-phi", po::value<size_t>(&min_phi), "Insert description")
            ("max-ita", po::value<size_t>(&max_ita), "Insert description")
            ("min-ita", po::value<size_t>(&min_ita), "Insert description")
            ("max-bi", po::value<T>(&max_bi), "Insert description")
            ("min-bi", po::value<T>(&min_bi), "Insert description");

    }
    void print_parameters() override
    {
        // spdlog::info("RandomFooSchemeTabuList - parameter min-phi: {}", min_phi);
        // spdlog::info("RandomFooSchemeTabuList - parameter max-phi: {}", max_phi);
        // spdlog::info("RandomFooSchemeTabuList - parameter min-ita: {}", min_ita);
        // spdlog::info("RandomFooSchemeTabuList - parameter max-ita: {}", max_ita);
        // spdlog::info("RandomFooSchemeTabuList - parameter min-bi: {}", min_bi);
        // spdlog::info("RandomFooSchemeTabuList - parameter max-bi: {}", max_bi);
        // spdlog::info("RandomFooSchemeTabuList - parameter current: {}", current);
        // spdlog::info("RandomFooSchemeTabuList - parameter last_it_update: {}", last_it_update);
        // spdlog::info("RandomFooSchemeTabuList - parameter max_current_size: {}", max_current_size);
        // spdlog::info("RandomFooSchemeTabuList - parameter min_initialized: {}", min_initialized);
        // spdlog::info("RandomFooSchemeTabuList - parameter max_initialized: {}", max_initialized);
        // // spdlog::info("RandomFooSchemeTabuList - parameter random seed: {}", rng.seed());
    }
    void initialize(Runner* r)
    {
        current = 0;
        last_it_update = 0;
        // set ita, phi, bi for the first round
        rng.seed(r->random_seed);
        std::uniform_int_distribution<size_t> phi_dist(min_phi, max_phi);
        phi = phi_dist(rng);
        
        std::uniform_int_distribution<size_t> ita_dist(min_ita, max_ita);
        ita = ita_dist(rng);
        
        std::uniform_int_distribution<T> bi_dist(min_bi, max_bi);
        bi = bi_dist(rng);
        
        max_current_size = ita;
        min_initialized = false;
        max_initialized = false;
    }

    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                spdlog::debug("RandomFooSchemeTabuList - move is tabu");
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        if(r->iteration == 0)
        {
            max_current_size = ita;
        }
        const Move& best_move = r->best_move_value->GetMove();
        const auto& best_move_cost = r->best_move_value->AggregatedCost();
        
        if (!min_initialized && !max_initialized)
        {
            min_o = best_move_cost;
            max_o = best_move_cost;
            min_initialized = true;
            max_initialized = true;
        }
        else
        {
            if (best_move_cost < min_o)
                min_o = best_move_cost;
            if(best_move_cost > max_o)
                max_o = best_move_cost;
        }
        
        if(r->iteration - last_it_update < phi)
        {
            add_or_update_current(r, best_move);
        }
        else
        {
            update_size(r, best_move);
        }
    }
    Move least_tabu(Runner* r)
    {
        return tabu_moves[current];
    }
protected:
    size_t current;
    size_t last_it_update;
    bool min_initialized, max_initialized;
    T min_o, max_o;
    std::vector<Move> tabu_moves;
    size_t max_current_size;
    mutable std::mt19937_64 rng;
    // parameters
    size_t max_ita, min_ita, ita;
    size_t max_phi, min_phi, phi;
    T max_bi, min_bi, bi;
    
    void add_or_update_current(Runner* r, Move best_move)
    {
#if !defined(NDEBUG)
        spdlog::debug("size is {} // current max size is {} // ita is ", tabu_moves.size(), max_current_size, ita);
#endif
        if (tabu_moves.size() < max_current_size)
        {
            tabu_moves.push_back(best_move);
            
        }
        else
        {
            tabu_moves[current] = best_move;
            current = (current + 1) % max_current_size;
        }
    }
    
    void update_size(Runner* r, Move best_move)
    {
        
        if (max_o - min_o < bi)
        {
            max_current_size = max_current_size + ita;
        }
        else
        {
            if (tabu_moves.size() > 0)
            {
                tabu_moves.erase(tabu_moves.begin() + current);
                max_current_size = max_current_size - 1;
                current = (current + 1) % max_current_size;
            }
        }
         max_initialized = false;
         min_initialized = false;
         last_it_update = r->iteration;
        
        std::uniform_int_distribution<size_t> phi_dist(min_phi, max_phi);
        phi = phi_dist(rng);
        
        std::uniform_int_distribution<size_t> ita_dist(min_ita, max_ita);
        ita = ita_dist(rng);
        
        std::uniform_int_distribution<T> bi_dist(min_bi, max_bi);
        bi = bi_dist(rng);
    }
};

// TODO: define the proper concept for AspirationCriterion
template <class Runner>
class AspirationByObjective : public Parametrized
{
public:
    bool is_tabu_status_overridden(Runner* r)
    {
        if (*(r->current_move_value) < *(r->best_solution_value))
        {
#if !defined(NDEBUG)
            spdlog::debug("AspirationByObjective - Tabu status overriden");
#endif
            return true;
            r->metric_aspiration_used + 1;
        }
        else
            return false;
    }
    void update(Runner* r)
    {}
    bool use_least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("AspirationByObjective - Use least tabu is false");
#endif
        return false;
    }
protected:
};

template <class Runner>
class AspirationByDefault : public Parametrized
{
public:
    bool is_tabu_status_overridden(Runner* r)
    {
        // you never override
        return false;
    }
    void update(Runner* r)
    {}
    bool use_least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("AspirationByDefault - Use least tabu is true");
#endif
        return true;
    }
protected:
};

// TODO: define the proper concept for StopExploration
template <class Runner>
class StopExplorationBestImprovement : public Parametrized
{
public:
    bool has_to_stop(Runner* r)
    {
        return false;
    }
    void update(Runner* r)
    {}
    void initialize(Runner* r)
    {}
protected:
};

template <class Runner>
class StopExplorationFirstImprovement : public Parametrized
{
public:
    bool has_to_stop(Runner* r)
    {
        if (*(r->current_move_value) < *(r->best_solution_value))
        {
#if !defined(NDEBUG)
            spdlog::debug("StopExplorationFirstImprovement - Stopping at best improvement");
#endif
            return true;
        }
        return false;
    }
    void update(Runner* r)
    {}
    void initialize(Runner* r)
    {}
protected:
};

template <class Runner>
class StopExplorationAspirationPlus : public Parametrized
{
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("theta-asp", po::value<double>(&theta), "Insert description")
            ("min", po::value<size_t>(&min), "Insert description")
            ("max", po::value<size_t>(&max), "Insert description")
            ("plus", po::value<size_t>(&plus), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("StopExplorationAspirationPlus - parameter theta: {}", theta);
        // spdlog::info("StopExplorationAspirationPlus - parameter min: {}", min);
        // spdlog::info("StopExplorationAspirationPlus - parameter max: {}", max);
        // spdlog::info("StopExplorationAspirationPlus - parameter plus: {}", plus);
    }
    bool has_to_stop(Runner* r)
    {
        return (number_of_neighbors >= max) || (first_found && number_of_neighbors - first_under_threshold >= plus && number_of_neighbors >= min);
    }
    void update(Runner* r)
    {
        number_of_neighbors++;
        if (!first_found && r->current_move_value->AggregatedCost() < threshold)
        {
#if !defined(NDEBUG)
            spdlog::debug("StopExplorationAspirationPlus - Found value under threshold");
#endif
            first_found = true;
            first_under_threshold = number_of_neighbors;
        }
    }
    void initialize(Runner* r)
    {
        // FIMXE: this will work only with aggregated cost, to think about for lexicographic and pareto objectives
        threshold = r->best_solution_value->AggregatedCost() * theta;
        number_of_neighbors = 0;
        first_found = false;
        assert(theta >= 1.0);
        assert(min > 0 && min < max);
    }
protected:
    T threshold;
    double theta;
    bool first_found;
    size_t min, max, plus;
    size_t number_of_neighbors, first_under_threshold;
};

// TODO: define the proper concept for SelectMove
template <class Runner>
class SelectMoveRandom : public Parametrized
{
    using Move = typename Runner::Move;
public:
    auto select(Runner* r)
    {
        return r->ne->CreateMoveValue(*r->current_solution_value, r->ne->RandomMove(r->current_solution_value->GetSolution()));
    }
protected:
};

template <class Runner>
class SelectMoveScanningAll : public Parametrized
{
    using Move = typename Runner::Move;
    using MoveValue = typename Runner::MoveValue;
public:
    auto select(Runner* r)
    {
        bool best_move_value_initialized = false;
        for (auto mv : r->ne->Neighborhood(r->current_solution_value->GetSolution()))
        {
            current_move_value = std::make_shared<MoveValue>(r->ne->CreateMoveValue(*(r->current_solution_value), mv));
            if (!best_move_value_initialized || *current_move_value < *best_move_value)
            {
                best_move_value = std::make_shared<MoveValue>(*current_move_value);
                best_move_value_initialized = true;
            }
        }
        return *best_move_value;
    }
protected:
    std::shared_ptr<MoveValue> best_move_value, current_move_value;
};

// TODO: define the proper concept for AcceptMove
template <class Runner>
class AcceptMoveAlways : public Parametrized
{
    using Move = typename Runner::Move;
public:
    bool accept(Runner* r)
    {
        return true;
    }
protected:
};

template <class Runner>
class AcceptMoveImproveOrEqual : public Parametrized
{
    using Move = typename Runner::Move;
public:
    bool accept(Runner* r)
    {
        
        return *(r->current_move_value) <= *(r->current_solution_value);
    }
protected:
};

template <class Runner>
class AcceptMoveImprove : public Parametrized
{
    using Move = typename Runner::Move;
public:
    bool accept(Runner* r)
    {
        return *(r->current_move_value) < *(r->current_solution_value);
    }
protected:
};

}

