#pragma once

#include <easylocal/check.hpp>
#include <easylocal/tester.hpp>

#include <ftxui/ftxui.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <ostream>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::tui
{

struct tester_options
{
    std::string title{"EasyLocal++ Tester"};
    std::uint64_t seed{};
};

namespace detail
{

template<class T>
concept ostream_insertable =
    requires(std::ostream& out, const T& value) {
        out << value;
    };

template<class T>
concept tuple_like =
    requires { typename std::tuple_size<std::remove_cvref_t<T>>::type; };

template<class T>
[[nodiscard]] auto value_text(const T& value) -> std::string
{
    if constexpr (ostream_insertable<T>)
    {
        std::ostringstream out;
        out << value;
        return out.str();
    }
    else if constexpr (requires { value.hard(); value.soft(); })
    {
        return "hard=" + value_text(value.hard()) +
               ", soft=" + value_text(value.soft());
    }
    else if constexpr (tuple_like<T>)
    {
        std::string result{"["};
        std::size_t index = 0;
        std::apply(
            [&](const auto&... entries) {
                ((result += (index++ == 0 ? "" : ", ") +
                            value_text(entries)),
                 ...);
            },
            value);
        result += ']';
        return result;
    }
    else
    {
        return "<not printable>";
    }
}

inline auto text_lines(const std::string& value) -> ftxui::Element
{
    ftxui::Elements lines;
    std::istringstream input{value};
    std::string line;
    while (std::getline(input, line))
    {
        lines.push_back(ftxui::text(line));
    }
    if (lines.empty())
    {
        lines.push_back(ftxui::text(""));
    }
    return ftxui::vbox(std::move(lines));
}

template<class App>
class tester_frontend
{
public:
    using tester_type = easylocal::Tester<App>;

    tester_frontend(tester_type& tester, tester_options options)
        : tester_{tester},
          options_{std::move(options)},
          rng_{options_.seed}
    {
        for (const auto name : tester_.runner_names())
        {
            runner_names_.emplace_back(name);
        }
    }

    void run()
    {
        using namespace ftxui;

        auto app = ftxui::App::TerminalOutput();
        auto controls = Container::Vertical({});

        if constexpr (tester_type::supports_input_loading)
        {
            controls->Add(Input(&input_path_, "instance file"));
            controls->Add(Button("Load instance", [this] { load_input(); }));
        }

        if constexpr (tester_type::supports_initial_solution)
        {
            controls->Add(Button("Initial solution", [this] { use_initial_solution(); }));
        }
        if constexpr (tester_type::supports_random_solution)
        {
            controls->Add(Button("Random solution", [this] { use_random_solution(); }));
        }

        if constexpr (tester_type::supports_solution_loading ||
                      tester_type::supports_solution_saving)
        {
            controls->Add(Input(&solution_path_, "solution file"));
        }
        if constexpr (tester_type::supports_solution_loading)
        {
            controls->Add(Button("Load solution", [this] { load_solution(); }));
        }
        if constexpr (tester_type::supports_solution_saving)
        {
            controls->Add(Button("Save solution", [this] { save_solution(); }));
        }

        controls->Add(Button("Check", [this] { check(); }));

        if constexpr (tester_type::supports_deterministic_moves)
        {
            controls->Add(Button("First move", [this] { first_move(); }));
            controls->Add(Button("Next move", [this] { next_move(); }));
        }
        if constexpr (tester_type::supports_random_moves)
        {
            controls->Add(Button("Random move", [this] { random_move(); }));
        }
        controls->Add(Button("Apply move", [this] { apply_move(); }));

        if (!runner_names_.empty())
        {
            controls->Add(Menu(&runner_names_, &runner_selected_));
            controls->Add(Button("Run runner", [this] { run_runner(); }));
        }

        controls->Add(Button("Quit", [&app] { app.Exit(); }));

        auto renderer = Renderer(controls, [this, controls] {
            return render(controls);
        });

        app.Loop(renderer);
    }

private:
    template<class Function>
    void perform(std::string_view label, Function&& function)
    {
        try
        {
            std::forward<Function>(function)();
        }
        catch (const std::exception& error)
        {
            status_ = std::string{label} + ": " + error.what();
        }
        catch (...)
        {
            status_ = std::string{label} + ": unknown error";
        }
    }

    [[nodiscard]] auto require_input(std::string_view action) -> bool
    {
        if (tester_.has_input())
        {
            return true;
        }
        status_ = std::string{action} + ": load an instance first";
        return false;
    }

    [[nodiscard]] auto require_solution(std::string_view action) -> bool
    {
        if (tester_.has_solution())
        {
            return true;
        }
        status_ = std::string{action} + ": choose or load a solution first";
        return false;
    }

    [[nodiscard]] auto require_move(std::string_view action) -> bool
    {
        if (tester_.has_move())
        {
            return true;
        }
        status_ = std::string{action} + ": select a move first";
        return false;
    }

    void load_input()
        requires tester_type::supports_input_loading
    {
        if (input_path_.empty())
        {
            status_ = "Load instance: enter a file name";
            return;
        }
        perform("Load instance", [this] {
            tester_.load_input(std::filesystem::path{input_path_});
            status_ = "Loaded instance: " + input_path_;
        });
    }

    void use_initial_solution()
        requires tester_type::supports_initial_solution
    {
        if (!require_input("Initial solution"))
        {
            return;
        }
        perform("Initial solution", [this] {
            tester_.use_initial_solution();
            status_ = "Initial solution selected";
        });
    }

    void use_random_solution()
        requires tester_type::supports_random_solution
    {
        if (!require_input("Random solution"))
        {
            return;
        }
        perform("Random solution", [this] {
            tester_.use_random_solution(rng_);
            status_ = "Random solution selected";
        });
    }

    void load_solution()
        requires tester_type::supports_solution_loading
    {
        if (!require_input("Load solution"))
        {
            return;
        }
        if (solution_path_.empty())
        {
            status_ = "Load solution: enter a file name";
            return;
        }
        perform("Load solution", [this] {
            tester_.load_solution(std::filesystem::path{solution_path_});
            status_ = "Loaded solution: " + solution_path_;
        });
    }

    void save_solution()
        requires tester_type::supports_solution_saving
    {
        if (!require_solution("Save solution"))
        {
            return;
        }
        if (solution_path_.empty())
        {
            status_ = "Save solution: enter a file name";
            return;
        }
        perform("Save solution", [this] {
            tester_.save_solution(std::filesystem::path{solution_path_});
            status_ = "Saved solution: " + solution_path_;
        });
    }

    void check()
    {
        if (!require_solution("Check"))
        {
            return;
        }
        perform("Check", [this] {
            const auto report = tester_.check();
            std::ostringstream out;
            easylocal::print_report(out, report);
            status_ = out.str();
        });
    }

    void first_move()
        requires tester_type::supports_deterministic_moves
    {
        if (!require_solution("First move"))
        {
            return;
        }
        perform("First move", [this] {
            status_ = tester_.use_first_move()
                          ? move_status("Selected first move")
                          : "First move: neighborhood is empty";
        });
    }

    void next_move()
        requires tester_type::supports_deterministic_moves
    {
        if (!require_solution("Next move"))
        {
            return;
        }
        perform("Next move", [this] {
            status_ = tester_.use_next_move()
                          ? move_status("Selected next move")
                          : "Next move: no further move";
        });
    }

    void random_move()
        requires tester_type::supports_random_moves
    {
        if (!require_solution("Random move"))
        {
            return;
        }
        perform("Random move", [this] {
            status_ = tester_.use_random_move(rng_)
                          ? move_status("Selected random move")
                          : "Random move: neighborhood produced no move";
        });
    }

    void apply_move()
    {
        if (!require_move("Apply move"))
        {
            return;
        }
        if (!tester_.move_is_valid())
        {
            status_ = "Apply move: selected move is invalid";
            return;
        }
        perform("Apply move", [this] {
            tester_.apply_move();
            status_ = "Move applied";
        });
    }

    void run_runner()
    {
        if (!require_solution("Run runner"))
        {
            return;
        }
        if (runner_names_.empty())
        {
            status_ = "Run runner: no runner registered";
            return;
        }
        perform("Run runner", [this] {
            const auto& name = runner_names_.at(
                static_cast<std::size_t>(runner_selected_));
            if (!tester_.run_runner(name))
            {
                status_ = "Run runner: runner not found: " + name;
                return;
            }
            status_ = "Runner completed: " + name;
        });
    }

    [[nodiscard]] auto move_status(std::string prefix) const -> std::string
    {
        if (!tester_.has_move())
        {
            return prefix;
        }
        const bool valid = tester_.move_is_valid();
        prefix += valid ? " (valid)" : " (invalid)";
        if (!valid)
        {
            return prefix;
        }

        prefix += ", incremental cost=" + value_text(tester_.evaluate_move());
        prefix += ", full cost=" + value_text(tester_.evaluate_move_fully());
        if constexpr (requires(const tester_type& tester) {
                          { tester.move_evaluation_matches_full() }
                              -> std::same_as<bool>;
                      })
        {
            prefix += tester_.move_evaluation_matches_full()
                          ? ", delta check=ok"
                          : ", delta check=FAILED";
        }
        return prefix;
    }

    [[nodiscard]] auto render_state() const -> ftxui::Element
    {
        using namespace ftxui;
        Elements lines;
        lines.push_back(text(
            std::string{"Input: "} + (tester_.has_input() ? "loaded" : "not loaded")));
        lines.push_back(text(
            std::string{"Solution: "} +
            (tester_.has_solution() ? "available" : "not selected")));

        if (tester_.has_solution())
        {
            const bool valid = tester_.is_valid();
            lines.push_back(text(
                std::string{"Valid: "} + (valid ? "yes" : "no")));
            if (valid)
            {
                lines.push_back(text("Cost: " + value_text(tester_.evaluate())));
            }
        }

        lines.push_back(text(
            std::string{"Move: "} + (tester_.has_move() ? "selected" : "none")));
        if (tester_.has_move())
        {
            lines.push_back(text(
                std::string{"Move valid: "} +
                (tester_.move_is_valid() ? "yes" : "no")));
        }

        lines.push_back(text(
            "Runners: " + std::to_string(runner_names_.size())));
        return vbox(std::move(lines));
    }

    [[nodiscard]] auto render(const ftxui::Component& controls) const
        -> ftxui::Element
    {
        using namespace ftxui;
        return vbox({
                   text(options_.title) | bold | center,
                   separator(),
                   hbox({
                       window(text(" Controls "), controls->Render()) | flex,
                       window(text(" State "), render_state()) | flex,
                   }) | flex,
                   window(text(" Status "), text_lines(status_)),
               }) |
               border;
    }

    tester_type& tester_;
    tester_options options_;
    typename tester_type::rng_type rng_;
    std::string input_path_;
    std::string solution_path_;
    std::string status_{"Ready"};
    std::vector<std::string> runner_names_;
    int runner_selected_{};
};

} // namespace detail

template<class App>
void run(easylocal::Tester<App>& tester, tester_options options = {})
{
    detail::tester_frontend<App>{tester, std::move(options)}.run();
}

} // namespace easylocal::tui
