// A schema is computed at compile time: an inverted range fails there, with
// its reason, rather than in validate() or at run time.
#include <easylocal/config/parameter_set.hpp>

struct Parameters
{
    double rate{0.5};

    static consteval auto parameter_schema()
    {
        return easylocal::config::fields(
            easylocal::config::field<"rate", &Parameters::rate>(
                "Rate",
                easylocal::config::range(1.0, 0.0)));
    }

    [[nodiscard]] easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};

int main()
{
    Parameters parameters;
    easylocal::config::parameter_set set;
    set.add(parameters);
}
