// What a seed reproduces: each run of a session draws its own generator from
// the session's, so the same seed and the same commands give the same runs in
// every frontend (Session, cli::run; the TextUI and REST run on a Session).

#include "../examples/tutorial/tsp.hpp"

#include <easylocal/app/app.hpp>
#include <easylocal/app/cli.hpp>
#include <easylocal/app/io.hpp>
#include <easylocal/app/session.hpp>

#include <cassert>
#include <cstdint>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef EASYLOCAL_TUTORIAL_INSTANCE
#error "EASYLOCAL_TUTORIAL_INSTANCE must name the tutorial's instance"
#endif

namespace
{

using namespace tutorial;

auto tsp_app()
{
    return easylocal::app("tsp")
        | (easylocal::solution_manager<TourManager>()
            | easylocal::component<TourLength>())
        | (easylocal::neighborhood<TwoOptExplorer>()
            | easylocal::delta<TourLength, TwoOptLengthDelta>())
        | easylocal::runner<RandomDescent>("descent", {.max_evaluations = 4});
}

// The solution a run of the descent leaves, as text.
std::string text_of(const Tsp& input, const Tour& tour)
{
    std::ostringstream out;
    easylocal::write_solution(input, tour, out);
    return out.str();
}

// What cli::run writes for a seed, from a random tour.
std::string cli_output(const std::uint64_t seed)
{
    std::vector<std::string> storage{
        "reproducibility",
        "--instance",
        EASYLOCAL_TUTORIAL_INSTANCE,
        "--seed",
        std::to_string(seed),
        "--runner",
        "descent",
        "--start",
        "random"};
    std::vector<char*> argv;
    for (auto& argument : storage)
        argv.push_back(argument.data());
    std::ostringstream out;
    std::ostringstream err;
    const auto status = easylocal::cli::run(
        tsp_app(),
        static_cast<int>(argv.size()),
        argv.data(),
        {.out = &out, .err = &err});
    assert(status == 0);
    return out.str();
}

} // namespace

int main()
{
    const auto input = easylocal::load_input<Tsp>(EASYLOCAL_TUTORIAL_INSTANCE);
    const auto application = tsp_app();
    std::set<std::string> outcomes;

    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        // A session: a random tour from its generator, then a run, which draws
        // its own generator from it.
        easylocal::Session session{application, input, seed};
        session.use_random_solution(session.rng());
        assert(session.run("descent"));
        const auto by_session = text_of(input, session.solution());

        // The same draws, by hand: the session's generator is a
        // std::mt19937_64 seeded with the seed.
        std::mt19937_64 rng{seed};
        const auto start =
            application.bind(input).solution_manager().random_solution(rng);
        std::mt19937_64 run_rng{rng()};
        const auto by_hand = application.run("descent", input, start, run_rng);
        assert(by_hand);
        assert(text_of(input, by_hand->solution) == by_session);

        // cli::run is a session: it writes the same solution, last.
        const auto output = cli_output(seed);
        assert(output.ends_with(by_session));
        outcomes.insert(by_session);

        // A second run of the session draws the next generator of its stream,
        // from the solution of the first.
        assert(session.run("descent"));
        std::mt19937_64 second_rng{rng()};
        const auto second =
            application.run("descent", input, by_hand->solution, second_rng);
        assert(second);
        assert(text_of(input, second->solution) == text_of(input, session.solution()));
    }

    // The seed matters: the runs differ between seeds.
    assert(outcomes.size() > 1);
    return 0;
}
