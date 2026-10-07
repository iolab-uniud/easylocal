#pragma once

/// \file
/// Plain-text configuration files: one path = value per line, whole-line #
/// comments, read as the overrides of a parameter_set; and the reader of a
/// file, which another format, such as the TOML adapter's, replaces.

#include <easylocal/config/overrides.hpp>
#include <easylocal/utils/detail/text.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

namespace easylocal::config
{

/// An error of a configuration file.
enum class config_file_error
{
    /// The file cannot be opened, or is a directory.
    open_error,
    /// A line that is not `path = value`, a comment or blank.
    malformed_line,
    /// A line with nothing before its `=`.
    empty_path,
    /// A path already set on an earlier line.
    duplicate_path,
    /// A text that is not valid in the format of the file, as a TOML reader
    /// reports it.
    parse_error,
    /// A value that is not a parameter value, such as a TOML date.
    unsupported_value,
};

/// An error of a configuration file, with the line that caused it.
struct config_file_diagnostic
{
    /// The kind of error.
    config_file_error error;
    /// The number of the line, from 1; 0 when it is not known, as when the
    /// file cannot be opened.
    std::size_t line{};
    /// The line as written; the name of a file that cannot be opened or
    /// parsed; the path of an `unsupported_value`.
    std::string text;
    /// A description of the error.
    std::string message;
    /// The number of the column, from 1; 0 when it is not known, as for a
    /// line of a plain-text file.
    std::size_t column{};
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

    /// Whether there are no diagnostics.
    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

/// Reads the overrides `path = value` of a configuration text, one per line.
///
/// Blank lines and lines that start with `#` are skipped: comments take whole
/// lines, and a `#` after a value belongs to the value. Paths and values are
/// trimmed of spaces, and a UTF-8 byte order mark at the start is skipped. A
/// line in error gives a diagnostic, and the reading goes on.
[[nodiscard]]
inline config_file_parse_result parse_config_text(std::string_view text)
{
    config_file_parse_result result{};
    if (text.starts_with("\xEF\xBB\xBF"))
        text.remove_prefix(3);
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
        const auto line = easylocal::detail::trim_space(raw_line);

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
                const auto path = easylocal::detail::trim_space(line.substr(0, equals));
                const auto value = easylocal::detail::trim_space(line.substr(equals + 1));

                if (path.empty())
                {
                    result.diagnostics.push_back({
                        .error = config_file_error::empty_path,
                        .line = line_number,
                        .text = std::string{raw_line},
                        .message = "parameter path must not be empty",
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
                            .message = "duplicate parameter path; first defined on line "
                                + std::to_string(position->second),
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
/// A file that cannot be opened, or a directory, gives an `open_error`
/// diagnostic.
[[nodiscard]]
inline config_file_parse_result load_config_file(const std::filesystem::path& path)
{
    const auto failure = [&](std::string message) {
        config_file_parse_result result{};
        result.diagnostics.push_back({
            .error = config_file_error::open_error,
            .line = 0,
            .text = easylocal::detail::utf8_text(path),
            .message = std::move(message),
        });
        return result;
    };
    // A directory opens as a stream on some systems, and reads as empty.
    if (std::error_code error; std::filesystem::is_directory(path, error))
        return failure("configuration file is a directory");

    std::ifstream input{path};
    if (!input)
        return failure("cannot open configuration file");

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse_config_text(buffer.str());
}

/// How a program reads the file given with `--config`: load_config_file, or
/// load_toml_file of the TOML adapter, or a reader of another format that
/// gives the overrides of the file and its errors.
using config_file_reader =
    std::function<config_file_parse_result(const std::filesystem::path&)>;

} // namespace easylocal::config
