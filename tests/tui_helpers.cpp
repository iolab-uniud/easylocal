#include <easylocal/tui/tester.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>

namespace
{

struct member_described
{
    [[nodiscard]] auto describe() const -> std::string
    {
        return "member";
    }
};

inline auto operator<<(std::ostream& out, const member_described&) -> std::ostream&
{
    return out << "stream";
}

namespace adl_case
{
struct value
{
};

[[nodiscard]] inline auto describe(const value&) -> std::string
{
    return "adl";
}

inline auto operator<<(std::ostream& out, const value&) -> std::ostream&
{
    return out << "stream";
}
} // namespace adl_case

struct stream_only
{
};

inline auto operator<<(std::ostream& out, const stream_only&) -> std::ostream&
{
    return out << "stream";
}

} // namespace

int main()
{
    using easylocal::tui::path_display_mode;
    using easylocal::tui::detail::directory_entries;
    using easylocal::tui::detail::display_path;
    using easylocal::tui::detail::editable_path;
    using easylocal::tui::detail::value_text;

    assert(value_text(member_described{}) == "member");
    assert(value_text(adl_case::value{}) == "adl");
    assert(value_text(stream_only{}) == "stream");

    const auto fixture =
        std::filesystem::current_path() / "easylocal-tui-helpers-fixture";
    std::filesystem::remove_all(fixture);
    std::filesystem::create_directories(fixture / "z-dir");
    std::filesystem::create_directories(fixture / "a-dir");
    std::ofstream{fixture / "z.txt"} << 'z';
    std::ofstream{fixture / "a.txt"} << 'a';

    const auto entries = directory_entries(fixture);
    assert(entries.size() == 4);
    assert(entries[0].directory);
    assert(entries[0].path.filename() == "a-dir");
    assert(entries[1].directory);
    assert(entries[1].path.filename() == "z-dir");
    assert(!entries[2].directory);
    assert(entries[2].path.filename() == "a.txt");
    assert(!entries[3].directory);
    assert(entries[3].path.filename() == "z.txt");

    const auto target = fixture / "a.txt";
    assert(display_path(target, path_display_mode::relative, fixture) == "a.txt");
    assert(display_path(target, path_display_mode::absolute, fixture) ==
           target.lexically_normal().string());
    assert(display_path(target, path_display_mode::both, fixture) ==
           "a.txt  [" + target.lexically_normal().string() + "]");
    assert(editable_path(target, path_display_mode::relative, fixture) == "a.txt");
    assert(editable_path(target, path_display_mode::both, fixture) == "a.txt");
    assert(editable_path(target, path_display_mode::absolute, fixture) ==
           target.lexically_normal().string());

    std::filesystem::remove_all(fixture);
    return 0;
}
