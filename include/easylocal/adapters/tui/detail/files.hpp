#pragma once

/// \file
/// The files of the interactive tester: paths relative to a base, and the
/// entries of a directory as its browser lists them.

#include <algorithm>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace easylocal::tui::detail
{

[[nodiscard]] inline std::string path_basename(std::string_view path)
{
    if (path.empty())
        return {};
    return std::filesystem::path{std::string{path}}.filename().string();
}

struct file_entry
{
    std::filesystem::path path;
    bool directory{};
};

[[nodiscard]] inline std::filesystem::path absolute_path_from(
    const std::filesystem::path& path,
    const std::filesystem::path& base = {})
{
    std::error_code error;
    auto effective_base = base;
    if (effective_base.empty())
    {
        effective_base = std::filesystem::current_path(error);
        if (error)
            return path.lexically_normal();
    }
    else if (!effective_base.is_absolute())
    {
        effective_base = std::filesystem::absolute(effective_base, error);
        if (error)
            return path.lexically_normal();
    }

    if (path.is_absolute())
        return path.lexically_normal();
    return (effective_base / path).lexically_normal();
}

[[nodiscard]] inline std::filesystem::path relative_path_from(
    const std::filesystem::path& path,
    const std::filesystem::path& base = {})
{
    const auto absolute = absolute_path_from(path, base);
    const auto absolute_base = absolute_path_from({}, base);
    const auto relative = absolute.lexically_relative(absolute_base);
    return relative.empty() ? absolute : relative;
}

[[nodiscard]] inline std::vector<file_entry> directory_entries(
    const std::filesystem::path& directory)
{
    std::error_code error;
    std::filesystem::directory_iterator iterator{directory, error};
    if (error)
    {
        throw std::filesystem::filesystem_error{
            "cannot browse directory",
            directory,
            error};
    }

    std::vector<file_entry> entries;
    for (const auto& entry : iterator)
    {
        std::error_code status_error;
        const bool is_directory = entry.is_directory(status_error);
        if (status_error)
            continue;
        entries.push_back({entry.path(), is_directory});
    }

    std::ranges::sort(entries, [](const file_entry& lhs, const file_entry& rhs) {
        if (lhs.directory != rhs.directory)
            return lhs.directory > rhs.directory;
        return lhs.path.filename().string() < rhs.path.filename().string();
    });
    return entries;
}

} // namespace easylocal::tui::detail
