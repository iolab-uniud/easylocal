#include <easylocal/app/app.hpp>
#include <easylocal/app/io.hpp>
#include <easylocal/app/session.hpp>
#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/helpers/neighborhood_union.hpp>
#include <easylocal/helpers/solution_manager.hpp>
#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/utils/generator.hpp>
#include <easylocal/utils/limit.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace
{

struct Move
{
};

template<class Input, class Solution>
class IoSolutionManager : public easylocal::solution_manager_base<Input, Solution>
{
public:
    using easylocal::solution_manager_base<Input, Solution>::solution_manager_base;

    [[nodiscard]]
    static auto is_valid(const Solution&) noexcept -> bool
    {
        return true;
    }
};

struct IoValue
{
    template<class Solution>
    [[nodiscard]]
    static auto evaluate(const Solution& solution) noexcept -> int
    {
        return solution.value;
    }
};

template<class SolutionManager>
class IoNeighborhood : public easylocal::neighborhood_explorer_base<SolutionManager, Move>
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

    static void make_move(typename SolutionManager::solution_type&, const Move&) noexcept
    {
    }

    // No moves: the tests read and write solutions only.
    [[nodiscard]] static easylocal::generator<Move> moves(
        const typename SolutionManager::solution_type&)
    {
        co_return;
    }
};

template<class Input, class Solution>
[[nodiscard]]
auto make_io_application(const char* name)
{
    using solution_manager_type = IoSolutionManager<Input, Solution>;
    using neighborhood_explorer_type = IoNeighborhood<solution_manager_type>;

    auto application =
        easylocal::app(name)
            .with_solution_manager(
                easylocal::solution_manager<solution_manager_type>()
                | easylocal::component<IoValue>())
            .with_neighborhood(easylocal::neighborhood<neighborhood_explorer_type>())
            .template with_runner<easylocal::runners::FirstImprovement>("fi");

    application.template runner_parameters<easylocal::runners::FirstImprovement>("fi")
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

[[maybe_unused]] inline auto operator>>(std::istream& in, Input& input) -> std::istream&
{
    in >> input.value;
    input.source = 3;
    return in;
}

struct Solution
{
    int value{};
    int source{};

    explicit Solution(const Input& input) : value{input.value} {}

    Solution(int value, int source) : value{value}, source{source} {}

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

[[maybe_unused]] inline auto operator>>(std::istream& in, Solution& solution)
    -> std::istream&
{
    in >> solution.value;
    solution.source = 3;
    return in;
}

[[maybe_unused]] inline auto operator<<(std::ostream& out, const Solution& solution)
    -> std::ostream&
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

[[maybe_unused]] inline auto operator>>(std::istream& in, Input& input) -> std::istream&
{
    in >> input.value;
    input.source = 3;
    return in;
}

struct Solution
{
    int value{};
    int source{};

    explicit Solution(const Input& input) : value{input.value} {}
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

[[maybe_unused]] inline auto operator>>(std::istream& in, Solution& solution)
    -> std::istream&
{
    in >> solution.value;
    solution.source = 3;
    return in;
}

[[maybe_unused]] inline auto operator<<(std::ostream& out, const Solution& solution)
    -> std::ostream&
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

    explicit Solution(const Input& input) : value{input.value} {}
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

namespace failing_io
{

// A Solution whose write hook fails the stream.
struct Solution
{
};

inline auto operator<<(std::ostream& out, const Solution&) -> std::ostream&
{
    out.setstate(std::ios::failbit);
    return out;
}

} // namespace failing_io

namespace associated_io
{

// A type with easylocal among its associated namespaces (a template argument):
// the argument-dependent lookup of its hooks also searches easylocal.
template<class Tag>
struct Tagged
{
    int value{};
};

template<class Tag>
auto operator<<(std::ostream& out, const Tagged<Tag>& tagged) -> std::ostream&
{
    out << "tagged:" << tagged.value;
    return out;
}

} // namespace associated_io

namespace describe_io
{

struct Member
{
    [[nodiscard]]
    static auto describe() -> std::string
    {
        return "member";
    }
};

[[maybe_unused]] inline auto operator<<(std::ostream& out, const Member&) -> std::ostream&
{
    return out << "stream";
}

struct Free
{
};

[[nodiscard]]
inline auto describe(const Free&) -> std::string
{
    return "free";
}

[[maybe_unused]] inline auto operator<<(std::ostream& out, const Free&) -> std::ostream&
{
    return out << "stream";
}

struct Streamed
{
};

inline auto operator<<(std::ostream& out, const Streamed&) -> std::ostream&
{
    return out << "stream";
}

struct Silent
{
};

} // namespace describe_io

static_assert(easylocal::readable_input<stream_io::Input>);
static_assert(easylocal::readable_solution<adl_io::Input, adl_io::Solution>);
static_assert(easylocal::writable_solution<static_io::Input, static_io::Solution>);
static_assert(easylocal::describable<int>);
static_assert(!easylocal::describable<describe_io::Silent>);
static_assert(
    !easylocal::describable<std::variant<describe_io::Free, describe_io::Silent>>);
static_assert(!easylocal::readable_input<describe_io::Silent>);

template<class Input, class Solution>
using io_app_type = decltype(make_io_application<Input, Solution>("io"));

template<class Input, class Solution>
using session_type = easylocal::Session<io_app_type<Input, Solution>>;

static_assert(session_type<static_io::Input, static_io::Solution>::supports_read_input);
static_assert(
    session_type<static_io::Input, static_io::Solution>::supports_read_solution);
static_assert(
    session_type<static_io::Input, static_io::Solution>::supports_write_solution);
static_assert(
    !session_type<static_io::Input, static_io::Solution>::supports_initial_solution);
static_assert(
    !session_type<static_io::Input, static_io::Solution>::supports_random_solution);

static_assert(session_type<adl_io::Input, adl_io::Solution>::supports_read_input);
static_assert(session_type<adl_io::Input, adl_io::Solution>::supports_read_solution);
static_assert(session_type<adl_io::Input, adl_io::Solution>::supports_write_solution);

static_assert(session_type<stream_io::Input, stream_io::Solution>::supports_read_input);
static_assert(
    session_type<stream_io::Input, stream_io::Solution>::supports_read_solution);
static_assert(
    session_type<stream_io::Input, stream_io::Solution>::supports_write_solution);

void static_member_io_has_priority()
{
    easylocal::Session session{
        make_io_application<static_io::Input, static_io::Solution>("static-io")};

    std::istringstream input_stream{"5"};
    session.read_input(input_stream);
    assert(session.input().value == 5);
    assert(session.input().source == 1);

    std::istringstream solution_stream{"7"};
    session.read_solution(solution_stream);
    assert(session.solution().value == 12);
    assert(session.solution().source == 1);

    std::ostringstream out;
    session.write_solution(out);
    assert(out.str() == "member:5:12");
}

void adl_io_has_priority_over_stream_operators()
{
    easylocal::Session session{
        make_io_application<adl_io::Input, adl_io::Solution>("adl-io")};

    std::istringstream input_stream{"11"};
    session.read_input(input_stream);
    assert(session.input().value == 11);
    assert(session.input().source == 2);

    std::istringstream solution_stream{"4"};
    session.read_solution(solution_stream);
    assert(session.solution().value == 15);
    assert(session.solution().source == 2);

    std::ostringstream out;
    session.write_solution(out);
    assert(out.str() == "adl:11:15");
}

void stream_operators_are_supported_as_fallbacks()
{
    easylocal::Session session{
        make_io_application<stream_io::Input, stream_io::Solution>("stream-io")};

    std::istringstream input_stream{"13"};
    session.read_input(input_stream);
    assert(session.input().value == 13);
    assert(session.input().source == 3);

    std::istringstream solution_stream{"6"};
    session.read_solution(solution_stream);
    assert(session.solution().value == 19);
    assert(session.solution().source == 3);

    std::ostringstream out;
    session.write_solution(out);
    assert(out.str() == "stream:19");
}

void file_overloads_delegate_to_the_same_protocol()
{
    const auto input_path = std::filesystem::path{"easylocal_session_io_input.tmp"};
    const auto solution_path = std::filesystem::path{"easylocal_session_io_solution.tmp"};
    const auto output_path = std::filesystem::path{"easylocal_session_io_output.tmp"};

    {
        std::ofstream out{input_path};
        out << 17;
    }
    {
        std::ofstream out{solution_path};
        out << 8;
    }

    easylocal::Session session{
        make_io_application<stream_io::Input, stream_io::Solution>("file-io")};
    session.load_input(input_path);
    session.load_solution(solution_path);
    session.save_solution(output_path);

    std::string contents;
    {
        // Closed before the removal below, which fails on an open file on
        // Windows.
        std::ifstream saved{output_path};
        std::getline(saved, contents);
    }
    assert(contents == "stream:25");

    std::filesystem::remove(input_path);
    std::filesystem::remove(solution_path);
    std::filesystem::remove(output_path);
}

void failed_reads_do_not_replace_current_state()
{
    easylocal::Session session{
        make_io_application<stream_io::Input, stream_io::Solution>("failure-io")};

    std::istringstream good_input{"21"};
    session.read_input(good_input);
    std::istringstream good_solution{"2"};
    session.read_solution(good_solution);

    bool input_failed = false;
    try
    {
        std::istringstream bad_input{"not-a-number"};
        session.read_input(bad_input);
    }
    catch (const std::runtime_error&)
    {
        input_failed = true;
    }
    assert(input_failed);
    assert(session.input().value == 21);
    assert(session.solution().value == 23);

    bool solution_failed = false;
    try
    {
        std::istringstream bad_solution{"not-a-number"};
        session.read_solution(bad_solution);
    }
    catch (const std::runtime_error&)
    {
        solution_failed = true;
    }
    assert(solution_failed);
    assert(session.solution().value == 23);
}

void free_functions_follow_the_same_protocol()
{
    std::istringstream input_stream{"5"};
    const auto input = easylocal::read_input<static_io::Input>(input_stream);
    assert(input.value == 5);
    assert(input.source == 1);

    std::istringstream solution_stream{"4"};
    const auto solution = easylocal::read_solution<adl_io::Solution>(
        adl_io::Input{.value = 11},
        solution_stream);
    assert(solution.value == 15);
    assert(solution.source == 2);

    std::ostringstream out;
    easylocal::write_solution(adl_io::Input{.value = 11}, solution, out);
    assert(out.str() == "adl:11:15");

    bool failed = false;
    try
    {
        std::istringstream bad_input{"not-a-number"};
        [[maybe_unused]] const auto ignored =
            easylocal::read_input<stream_io::Input>(bad_input);
    }
    catch (const std::runtime_error&)
    {
        failed = true;
    }
    assert(failed);
}

void free_file_functions_follow_the_same_protocol()
{
    const auto input_path = std::filesystem::path{"easylocal_io_input.tmp"};
    const auto output_path = std::filesystem::path{"easylocal_io_output.tmp"};
    {
        std::ofstream out{input_path};
        out << 17;
    }

    const auto input = easylocal::load_input<stream_io::Input>(input_path);
    assert(input.value == 17);
    // The Input file read again as a Solution: 17 + 17.
    const auto solution =
        easylocal::load_solution<stream_io::Solution>(input, input_path);
    assert(solution.value == 34);
    easylocal::save_solution(input, solution, output_path);

    std::string contents;
    {
        std::ifstream saved{output_path};
        std::getline(saved, contents);
    }
    assert(contents == "stream:34");

    std::filesystem::remove(input_path);
    std::filesystem::remove(output_path);
}

void free_file_errors_name_the_file()
{
    const auto bad_path = std::filesystem::path{"easylocal_io_bad.tmp"};
    {
        std::ofstream out{bad_path};
        out << "not-a-number";
    }
    std::string message;
    try
    {
        [[maybe_unused]] const auto ignored =
            easylocal::load_input<stream_io::Input>(bad_path);
    }
    catch (const std::runtime_error& error)
    {
        message = error.what();
    }
    assert(message.find(bad_path.string()) != std::string::npos);
    std::filesystem::remove(bad_path);

    message.clear();
    try
    {
        [[maybe_unused]] const auto ignored = easylocal::load_input<stream_io::Input>(
            std::filesystem::path{"easylocal_io_missing.tmp"});
    }
    catch (const std::runtime_error& error)
    {
        message = error.what();
    }
    assert(message.find("easylocal_io_missing.tmp") != std::string::npos);
}

void free_file_write_errors_name_the_file()
{
    const auto output_path = std::filesystem::path{"easylocal_io_failed_write.tmp"};
    std::string message;
    try
    {
        easylocal::save_solution(stream_io::Input{}, failing_io::Solution{}, output_path);
    }
    catch (const std::runtime_error& error)
    {
        message = error.what();
    }
    assert(message.find(output_path.string()) != std::string::npos);
    std::filesystem::remove(output_path);
}

void hooks_of_types_associated_with_easylocal()
{
    using tagged = associated_io::Tagged<easylocal::limit>;
    assert(easylocal::describe(tagged{.value = 4}) == "tagged:4");
    std::ostringstream out;
    easylocal::write_solution(stream_io::Input{}, tagged{.value = 5}, out);
    assert(out.str() == "tagged:5");
}

void describe_prefers_the_value_hooks()
{
    assert(easylocal::describe(describe_io::Member{}) == "member");
    assert(easylocal::describe(describe_io::Free{}) == "free");
    assert(easylocal::describe(describe_io::Streamed{}) == "stream");
    assert(easylocal::describe(42) == "42");
}

} // namespace

void variants_and_union_moves_are_described_by_what_they_hold()
{
    using move_type = std::variant<
        easylocal::detail::tagged_neighborhood_move<0, describe_io::Free>,
        easylocal::detail::tagged_neighborhood_move<1, describe_io::Streamed>>;
    static_assert(easylocal::describable<move_type>);
    assert(easylocal::describe(move_type{std::in_place_index<0>}) == "free");
    assert(easylocal::describe(move_type{std::in_place_index<1>}) == "stream");
    const std::variant<int, describe_io::Member> plain{describe_io::Member{}};
    assert(easylocal::describe(plain) == "member");
}

int main()
{
    static_member_io_has_priority();
    adl_io_has_priority_over_stream_operators();
    stream_operators_are_supported_as_fallbacks();
    file_overloads_delegate_to_the_same_protocol();
    failed_reads_do_not_replace_current_state();
    free_functions_follow_the_same_protocol();
    free_file_functions_follow_the_same_protocol();
    free_file_errors_name_the_file();
    free_file_write_errors_name_the_file();
    describe_prefers_the_value_hooks();
    hooks_of_types_associated_with_easylocal();
    variants_and_union_moves_are_described_by_what_they_hold();
}
