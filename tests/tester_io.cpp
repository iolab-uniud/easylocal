#include <easylocal/app/app.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/app/tester.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>

namespace
{

struct Move
{
};

template<class Input, class Solution>
class IoSolutionManager
    : public easylocal::solution_manager_base<Input, Solution>
{
public:
    using easylocal::solution_manager_base<Input, Solution>::solution_manager_base;
    using cost_type = int;

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }

    [[nodiscard]]
    static auto evaluate(const Solution& solution) noexcept -> cost_type
    {
        return solution.value;
    }
};

template<class SolutionManager>
class IoNeighborhood
    : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
{
public:
    using easylocal::neighborhood_explorer_base<
        SolutionManager,
        Move>::neighborhood_explorer_base;

    [[nodiscard]]
    static auto is_valid(
        const typename SolutionManager::solution_type&,
        const Move&) noexcept -> bool
    {
        return true;
    }

    static void make_move(
        typename SolutionManager::solution_type&,
        const Move&) noexcept
    {
    }
};

template<class Input, class Solution>
[[nodiscard]]
auto make_io_application(const char* name)
{
    using solution_manager_type = IoSolutionManager<Input, Solution>;
    using neighborhood_type = IoNeighborhood<solution_manager_type>;

    auto application = easylocal::app(name)
        .template solution_manager<solution_manager_type>()
        .template neighborhood<neighborhood_type>()
        .template runner<easylocal::runners::FirstImprovement>("fi");

    application
        .template runner_config<easylocal::runners::FirstImprovement>()
        .max_evaluations = 1;

    return application;
}

namespace static_io
{

struct Input
{
    int value{};
    int source{};

    [[nodiscard]]
    static auto read(std::istream& in) -> Input
    {
        Input input;
        in >> input.value;
        input.source = 1;
        return input;
    }
};

inline auto operator>>(std::istream& in, Input& input) -> std::istream&
{
    in >> input.value;
    input.source = 3;
    return in;
}

struct Solution
{
    int value{};
    int source{};

    explicit Solution(const Input& input)
        : value{input.value}
    {
    }

    Solution(int value, int source)
        : value{value}, source{source}
    {
    }

    [[nodiscard]]
    static auto read(const Input& input, std::istream& in) -> Solution
    {
        int value{};
        in >> value;
        return Solution{input.value + value, 1};
    }

    void write(const Input& input, std::ostream& out) const
    {
        out << "member:" << input.value << ':' << value;
    }
};

inline auto operator>>(std::istream& in, Solution& solution) -> std::istream&
{
    in >> solution.value;
    solution.source = 3;
    return in;
}

inline auto operator<<(std::ostream& out, const Solution& solution) -> std::ostream&
{
    out << "stream:" << solution.value;
    return out;
}

} // namespace static_io

namespace adl_io
{

struct Input
{
    int value{};
    int source{};
};

[[nodiscard]]
inline auto read_input(std::type_identity<Input>, std::istream& in) -> Input
{
    Input input;
    in >> input.value;
    input.source = 2;
    return input;
}

inline auto operator>>(std::istream& in, Input& input) -> std::istream&
{
    in >> input.value;
    input.source = 3;
    return in;
}

struct Solution
{
    int value{};
    int source{};

    explicit Solution(const Input& input)
        : value{input.value}
    {
    }
};

[[nodiscard]]
inline auto read_solution(const Input& input, std::istream& in) -> Solution
{
    int value{};
    in >> value;
    Solution solution{input};
    solution.value += value;
    solution.source = 2;
    return solution;
}

inline void write_solution(
    const Input& input,
    const Solution& solution,
    std::ostream& out)
{
    out << "adl:" << input.value << ':' << solution.value;
}

inline auto operator>>(std::istream& in, Solution& solution) -> std::istream&
{
    in >> solution.value;
    solution.source = 3;
    return in;
}

inline auto operator<<(std::ostream& out, const Solution& solution) -> std::ostream&
{
    out << "stream:" << solution.value;
    return out;
}

} // namespace adl_io

namespace stream_io
{

struct Input
{
    int value{};
    int source{};
};

inline auto operator>>(std::istream& in, Input& input) -> std::istream&
{
    in >> input.value;
    input.source = 3;
    return in;
}

struct Solution
{
    int value{};
    int source{};

    explicit Solution(const Input& input)
        : value{input.value}
    {
    }
};

inline auto operator>>(std::istream& in, Solution& solution) -> std::istream&
{
    int value{};
    in >> value;
    solution.value += value;
    solution.source = 3;
    return in;
}

inline auto operator<<(std::ostream& out, const Solution& solution) -> std::ostream&
{
    out << "stream:" << solution.value;
    return out;
}

} // namespace stream_io

template<class Input, class Solution>
using io_app_type = decltype(make_io_application<Input, Solution>("io"));

template<class Input, class Solution>
using tester_type = easylocal::Tester<io_app_type<Input, Solution>>;

static_assert(tester_type<static_io::Input, static_io::Solution>::supports_input_loading);
static_assert(tester_type<static_io::Input, static_io::Solution>::supports_solution_loading);
static_assert(tester_type<static_io::Input, static_io::Solution>::supports_solution_saving);
static_assert(!tester_type<static_io::Input, static_io::Solution>::supports_initial_solution);
static_assert(!tester_type<static_io::Input, static_io::Solution>::supports_random_solution);

static_assert(tester_type<adl_io::Input, adl_io::Solution>::supports_input_loading);
static_assert(tester_type<adl_io::Input, adl_io::Solution>::supports_solution_loading);
static_assert(tester_type<adl_io::Input, adl_io::Solution>::supports_solution_saving);

static_assert(tester_type<stream_io::Input, stream_io::Solution>::supports_input_loading);
static_assert(tester_type<stream_io::Input, stream_io::Solution>::supports_solution_loading);
static_assert(tester_type<stream_io::Input, stream_io::Solution>::supports_solution_saving);

void static_member_io_has_priority()
{
    easylocal::Tester tester{
        make_io_application<static_io::Input, static_io::Solution>("static-io")};

    std::istringstream input_stream{"5"};
    tester.load_input(input_stream);
    assert(tester.input().value == 5);
    assert(tester.input().source == 1);

    std::istringstream solution_stream{"7"};
    tester.load_solution(solution_stream);
    assert(tester.solution().value == 12);
    assert(tester.solution().source == 1);

    std::ostringstream out;
    tester.save_solution(out);
    assert(out.str() == "member:5:12");
}

void adl_io_has_priority_over_stream_operators()
{
    easylocal::Tester tester{
        make_io_application<adl_io::Input, adl_io::Solution>("adl-io")};

    std::istringstream input_stream{"11"};
    tester.load_input(input_stream);
    assert(tester.input().value == 11);
    assert(tester.input().source == 2);

    std::istringstream solution_stream{"4"};
    tester.load_solution(solution_stream);
    assert(tester.solution().value == 15);
    assert(tester.solution().source == 2);

    std::ostringstream out;
    tester.save_solution(out);
    assert(out.str() == "adl:11:15");
}

void stream_operators_are_supported_as_fallbacks()
{
    easylocal::Tester tester{
        make_io_application<stream_io::Input, stream_io::Solution>("stream-io")};

    std::istringstream input_stream{"13"};
    tester.load_input(input_stream);
    assert(tester.input().value == 13);
    assert(tester.input().source == 3);

    std::istringstream solution_stream{"6"};
    tester.load_solution(solution_stream);
    assert(tester.solution().value == 19);
    assert(tester.solution().source == 3);

    std::ostringstream out;
    tester.save_solution(out);
    assert(out.str() == "stream:19");
}

void file_overloads_delegate_to_the_same_protocol()
{
    const auto input_path = std::filesystem::path{"easylocal_tester_io_input.tmp"};
    const auto solution_path = std::filesystem::path{"easylocal_tester_io_solution.tmp"};
    const auto output_path = std::filesystem::path{"easylocal_tester_io_output.tmp"};

    {
        std::ofstream out{input_path};
        out << 17;
    }
    {
        std::ofstream out{solution_path};
        out << 8;
    }

    easylocal::Tester tester{
        make_io_application<stream_io::Input, stream_io::Solution>("file-io")};
    tester.load_input(input_path);
    tester.load_solution(solution_path);
    tester.save_solution(output_path);

    std::ifstream saved{output_path};
    std::string contents;
    std::getline(saved, contents);
    assert(contents == "stream:25");

    std::filesystem::remove(input_path);
    std::filesystem::remove(solution_path);
    std::filesystem::remove(output_path);
}

void failed_reads_do_not_replace_current_state()
{
    easylocal::Tester tester{
        make_io_application<stream_io::Input, stream_io::Solution>("failure-io")};

    std::istringstream good_input{"21"};
    tester.load_input(good_input);
    std::istringstream good_solution{"2"};
    tester.load_solution(good_solution);

    bool input_failed = false;
    try
    {
        std::istringstream bad_input{"not-a-number"};
        tester.load_input(bad_input);
    }
    catch (const std::runtime_error&)
    {
        input_failed = true;
    }
    assert(input_failed);
    assert(tester.input().value == 21);
    assert(tester.solution().value == 23);

    bool solution_failed = false;
    try
    {
        std::istringstream bad_solution{"not-a-number"};
        tester.load_solution(bad_solution);
    }
    catch (const std::runtime_error&)
    {
        solution_failed = true;
    }
    assert(solution_failed);
    assert(tester.solution().value == 23);
}

} // namespace

int main()
{
    static_member_io_has_priority();
    adl_io_has_priority_over_stream_operators();
    stream_operators_are_supported_as_fallbacks();
    file_overloads_delegate_to_the_same_protocol();
    failed_reads_do_not_replace_current_state();
}
