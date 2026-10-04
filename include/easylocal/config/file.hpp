#pragma once

/// \file
/// Plain-text configuration files: one path = value per line, # comments,
/// read as the overrides of a parameter_set.

#include <easylocal/config/overrides.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// An error of a configuration file.
enum class config_file_error
{
    /// The file cannot be opened.
    open_error,
    /// A line that is not `path = value`, a comment or blank.
    malformed_line,
    /// A line with nothing before its `=`.
    empty_path,
    /// A path already set on an earlier line.
    duplicate_path,
};

/// An error of a configuration file, with the line that caused it.
struct config_file_diagnostic
{
    /// The kind of error.
    config_file_error error;
    /// The number of the line, from 1; 0 when the file cannot be opened.
    std::size_t line{};
    /// The line as written, or the path of a file that cannot be opened.
    std::string text;
    /// A description of the error.
    std::string message;
};

/// The overrides of a configuration file, and its errors.
///
/// It converts to true when there are no errors.
struct config_file_parse_result
{
    /// The overrides `path = value`, in the order of the lines.
    std::vector<owned_text_override> overrides;
    /// The errors, in the order of the lines.
    std::vector<config_file_diagnostic> diagnostics;

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

/// Reads the overrides `path = value` of a configuration text, one per line.
///
/// Blank lines and lines that start with `#` are skipped; paths and values are
/// trimmed of spaces. A line in error gives a diagnostic, and the reading goes
/// on.
[[nodiscard]]
inline config_file_parse_result parse_config_text(const std::string_view text)
{
    config_file_parse_result result{};
    std::unordered_map<std::string, std::size_t> first_definition;

    std::size_t line_number = 0;
    std::size_t cursor = 0;

    while (cursor <= text.size())
    {
        ++line_number;
        const auto newline = text.find('\n', cursor);
        const auto raw_line = newline == std::string_view::npos
            ? text.substr(cursor)
            : text.substr(cursor, newline - cursor);
        const auto line = detail::trim_ascii_space(raw_line);

        if (!line.empty() && !line.starts_with('#'))
        {
            const auto equals = line.find('=');
            if (equals == std::string_view::npos)
            {
                result.diagnostics.push_back({
                    .error = config_file_error::malformed_line,
                    .line = line_number,
                    .text = std::string{raw_line},
                    .message = "expected 'path = value'",
                });
            }
            else
            {
                const auto path = detail::trim_ascii_space(line.substr(0, equals));
                const auto value = detail::trim_ascii_space(line.substr(equals + 1));

                if (path.empty())
                {
                    result.diagnostics.push_back({
                        .error = config_file_error::empty_path,
                        .line = line_number,
                        .text = std::string{raw_line},
                        .message = "configuration path must not be empty",
                    });
                }
                else
                {
                    const auto [position, inserted] = first_definition.emplace(
                        std::string{path},
                        line_number);
                    if (!inserted)
                    {
                        result.diagnostics.push_back({
                            .error = config_file_error::duplicate_path,
                            .line = line_number,
                            .text = std::string{raw_line},
                            .message = "duplicate configuration path; first defined on line " +
                                std::to_string(position->second),
                        });
                    }

                    result.overrides.push_back({
                        .path = std::string{path},
                        .value = std::string{value},
                    });
                }
            }
        }

        if (newline == std::string_view::npos)
        {
            break;
        }
        cursor = newline + 1;
    }

    return result;
}

/// Reads the overrides of a configuration file, as parse_config_text does.
///
/// A file that cannot be opened gives an `open_error` diagnostic.
[[nodiscard]]
inline config_file_parse_result load_config_file(const std::filesystem::path& path)
{
    std::ifstream input{path};
    if (!input)
    {
        config_file_parse_result result{};
        result.diagnostics.push_back({
            .error = config_file_error::open_error,
            .line = 0,
            .text = path.string(),
            .message = "cannot open configuration file",
        });
        return result;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse_config_text(buffer.str());
}

} // namespace easylocal::config
