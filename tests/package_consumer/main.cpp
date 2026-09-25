#include <easylocal/easylocal.hpp>

#if EASYLOCAL_TEST_CONFIG_TOML
#include <easylocal/config/toml.hpp>
#endif

#if __cplusplus < 202100L
#error "EasyLocal::Core must propagate a C++23 compile requirement"
#endif

int main()
{
#if EASYLOCAL_TEST_CONFIG_TOML
    const auto parsed = easylocal::config::parse_toml_text(
        "[solver]\niterations = 42\n");
    if (!parsed || parsed.overrides.size() != 1 ||
        parsed.overrides.front().path != "solver.iterations")
    {
        return 1;
    }
#endif
    return 0;
}
