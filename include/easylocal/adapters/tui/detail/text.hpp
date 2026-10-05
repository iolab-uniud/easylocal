#pragma once

/// \file
/// The text of the interactive tester: values as it shows them, lines wrapped
/// to the terminal, its scrollable viewer, and the numbers of its fields.

#include <easylocal/app/session.hpp>
#include <easylocal/cost.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <ftxui/ftxui.hpp>
#include <ftxui/screen/string.hpp>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace easylocal::tui::detail
{

template<class T>
concept tuple_like = requires { typename std::tuple_size<std::remove_cvref_t<T>>::type; };

template<class T>
concept named_object = requires(const T& value) {
    { value.name() } -> std::convertible_to<std::string_view>;
};

template<class T>
[[nodiscard]] std::string object_name(const T& value)
{
    if constexpr (named_object<T>)
        return std::string{value.name()};
    else
        return "<unnamed neighborhood>";
}

// A value as the tester shows it: as the Session's reports write it (a cost as
// the target field reads it, [hard, soft], [v1, v2, ...]; anything else by its
// describe hook or operator<<), and a composite whose parts are printable,
// such as a cost of described levels or a tuple, part by part.
template<class T>
[[nodiscard]] std::string value_text(const T& value)
{
    if constexpr (easylocal::cost::text_readable<T> || easylocal::describable<T>)
    {
        return easylocal::detail::report_text(value);
    }
    else if constexpr (easylocal::cost::hierarchical_type<T>)
    {
        return "[" + value_text(value.hard()) + ", " + value_text(value.soft()) + "]";
    }
    else if constexpr (easylocal::cost::lexicographic_type<T>
        || easylocal::cost::pareto_type<T>)
    {
        std::string result{"["};
        [&]<std::size_t... Index>(std::index_sequence<Index...>) {
            std::size_t emitted = 0;
            ((result +=
                 (emitted++ == 0 ? "" : ", ") + value_text(value.template get<Index>())),
                ...);
        }(std::make_index_sequence<std::remove_cvref_t<T>::levels>{});
        result += ']';
        return result;
    }
    else if constexpr (requires {
                           value.hard();
                           value.soft();
                       })
    {
        return "[" + value_text(value.hard()) + ", " + value_text(value.soft()) + "]";
    }
    else if constexpr (tuple_like<T>)
    {
        std::string result{"["};
        std::size_t index = 0;
        std::apply(
            [&](const auto&... entries) {
                ((result += (index++ == 0 ? "" : ", ") + value_text(entries)), ...);
            },
            value);
        result += ']';
        return result;
    }
    else
    {
        return "<not printable; add describe() or operator<<>";
    }
}

[[nodiscard]] inline std::string truncate_text(std::string value, const std::size_t limit)
{
    if (limit == 0 || value.size() <= limit)
        return value;
    value.resize(limit);
    value += "\n... <truncated>";
    return value;
}

[[nodiscard]] inline std::vector<std::string> split_text_lines(std::string_view value)
{
    std::vector<std::string> lines;
    std::istringstream input{std::string{value}};
    for (std::string line; std::getline(input, line);)
        lines.push_back(std::move(line));
    if (!value.empty() && value.back() == '\n')
        lines.emplace_back();
    if (lines.empty())
        lines.emplace_back();
    return lines;
}

// A text as a column of paragraphs, one per line.
inline ftxui::Element text_lines(const std::string_view value)
{
    ftxui::Elements lines;
    for (auto& line : split_text_lines(value))
        lines.push_back(ftxui::paragraph(std::move(line)));
    return ftxui::vbox(std::move(lines));
}

[[nodiscard]] inline std::vector<std::string> wrap_text_lines(
    std::string_view value,
    const std::size_t width)
{
    const auto effective_width = static_cast<int>(std::max<std::size_t>(1, width));
    std::vector<std::string> wrapped;

    const auto trim_left = [](std::string& text) {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
            text.erase(text.begin());
    };
    const auto trim_right = [](std::string& text) {
        while (!text.empty() && (text.back() == ' ' || text.back() == '\t'))
            text.pop_back();
    };

    for (auto logical_line : split_text_lines(value))
    {
        if (logical_line.empty())
        {
            wrapped.emplace_back();
            continue;
        }

        std::string current;
        std::size_t last_break{};
        for (const auto& glyph : ftxui::Utf8ToGlyphs(logical_line))
        {
            if (current.empty() && (glyph == " " || glyph == "\t"))
                continue;
            while (!current.empty()
                && ftxui::string_width(current + glyph) > effective_width)
            {
                if (last_break != 0)
                {
                    auto line = current.substr(0, last_break);
                    trim_right(line);
                    wrapped.push_back(std::move(line));
                    current.erase(0, last_break);
                    trim_left(current);
                }
                else
                {
                    wrapped.push_back(std::move(current));
                    current.clear();
                }
                last_break = 0;
            }

            // The overflow above may have emitted the whole current line.
            // In that case a whitespace glyph that triggered the wrap belongs
            // to the separator, not to the beginning of the next visual line.
            if (current.empty() && (glyph == " " || glyph == "\t"))
                continue;

            current += glyph;
            if (glyph == " " || glyph == "\t" || glyph == "," || glyph == ";")
                last_break = current.size();
        }

        if (!current.empty())
        {
            trim_right(current);
            wrapped.push_back(std::move(current));
        }
    }

    if (wrapped.empty())
        wrapped.emplace_back();
    return wrapped;
}

[[nodiscard]] inline int terminal_available_width(const int margin = 4) noexcept
{
    const auto dimensions = ftxui::Terminal::Size();
    return (std::max)(1, dimensions.dimx - margin);
}

[[nodiscard]] inline int terminal_available_height(const int margin = 2) noexcept
{
    const auto dimensions = ftxui::Terminal::Size();
    return (std::max)(1, dimensions.dimy - margin);
}

[[nodiscard]] constexpr int page_scroll_selection(
    const int selected,
    const std::size_t count,
    const int delta) noexcept
{
    if (count == 0)
        return 0;
    const auto last = static_cast<int>(count - 1);
    return std::clamp(selected + delta, 0, last);
}

// A number as text, spaces and tabs around it allowed (std::from_chars):
// empty when the text is not one, or has more. The seed, the seconds and the
// evaluations of the Run page are read alike.
template<class Number>
[[nodiscard]] std::optional<Number> number_from_text(const std::string_view text)
{
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos)
        return std::nullopt;
    const auto last = text.find_last_not_of(" \t");
    Number value{};
    const auto* const end = text.data() + last + 1;
    const auto [parsed, error] = std::from_chars(text.data() + first, end, value);
    if (error != std::errc{} || parsed != end)
        return std::nullopt;
    return value;
}

// A number of seconds as text: empty when it is not a non-negative number.
[[nodiscard]] inline std::optional<double> seconds_text(const std::string_view text)
{
    const auto seconds = number_from_text<double>(text);
    if (!seconds || !(*seconds >= 0.0) || !std::isfinite(*seconds))
        return std::nullopt;
    return seconds;
}

// A count as text: empty when it is not a non-negative whole number.
[[nodiscard]] inline std::optional<std::size_t> count_text(const std::string_view text)
{
    return number_from_text<std::size_t>(text);
}

// Seconds as the progress label shows them: one decimal, such as 12.3s.
[[nodiscard]] inline std::string seconds_label(const double seconds)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << seconds << 's';
    return out.str();
}

// A scrollable text wrapped to a width: the Input and solution windows. The
// lines are wrapped again only when the width changes.
class text_viewer
{
public:
    // Shows a new text from its first line.
    void show(std::string value, const std::size_t width)
    {
        text_ = std::move(value);
        wrap_width_ = 0;
        refresh(width);
        selected_ = 0;
    }

    void refresh(const std::size_t width)
    {
        if (wrap_width_ == width)
            return;
        wrap_width_ = width;
        lines_ = wrap_text_lines(text_, width);
        scroll(0);
    }

    void scroll(const int delta)
    {
        selected_ = page_scroll_selection(selected_, lines_.size(), delta);
    }

    // The lines as a vertical menu, which moves the selected line.
    [[nodiscard]] ftxui::Component menu()
    {
        return ftxui::Menu(&lines_, &selected_, ftxui::MenuOption::Vertical());
    }

private:
    std::string text_;
    std::vector<std::string> lines_{""};
    std::size_t wrap_width_{};
    int selected_{};
};

} // namespace easylocal::tui::detail
