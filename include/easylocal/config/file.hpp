#pragma once

// Plain-text configuration files: one path = value per line, # comments,
// read as the overrides of a parameter_set.

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

enum class config_file_error
{
    open_error,
    malformed_line,
    empty_path,
    duplicate_path,
};

struct config_file_diagnostic
{
    config_file_error error;
    std::size_t line{};
    std::string text;
    std::string message;
};

struct config_file_parse_result
{
    std::vector<owned_text_override> overrides;
    std::vector<config_file_diagnostic> diagnostics;

    [[nodiscard]]
    explicit operator bool() const noexcept
    {
        return diagnostics.empty();
    }
};

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
