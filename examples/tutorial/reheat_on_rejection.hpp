#pragma once

#include <easylocal/config/parameters_base.hpp>
#include <easylocal/runners/simulated_annealing.hpp>

#include <cstddef>
#include <span>

namespace tutorial
{

// [rejection-parameters]
// The cooling schedule and the trigger for a bounded number of reheats.
struct ReheatOnRejectionParameters
{
    easylocal::runners::temperature::ClassicParameters descent{};
    std::size_t rejections_before_reheat{100};
    std::size_t allowed_reheats{2};

    static consteval auto parameter_schema()
    {
        namespace cfg = easylocal::config;
        return cfg::fields(
            cfg::group<"descent", &ReheatOnRejectionParameters::descent>(
                "The geometric cooling schedule"),
            cfg::field<
                "rejections_before_reheat",
                &ReheatOnRejectionParameters::rejections_before_reheat>(
                "Consecutive rejected proposals before reheating",
                cfg::range(1, easylocal::unlimited)),
            cfg::field<"allowed_reheats", &ReheatOnRejectionParameters::allowed_reheats>(
                "Maximum reheats (0: cooling only)",
                cfg::range(0, easylocal::unlimited)));
    }

    easylocal::config::validation_result validate() const
    {
        return easylocal::config::check_schema(*this);
    }
};
// [rejection-parameters]

// [rejection-policy]
// Restart geometric cooling after a streak of rejected proposals.
class ReheatOnRejection : public easylocal::parameters_base<ReheatOnRejectionParameters>
{
public:
    explicit ReheatOnRejection(const ReheatOnRejectionParameters& parameters)
        : parameters_base{easylocal::config::require_valid(parameters)},
          initial_{parameters.descent},
          descent_{initial_}
    {
    }

    void reset()
    {
        descent_ = initial_;
        descent_.reset();
        rejected_ = 0;
        reheats_ = 0;
    }

    double temperature() const
    {
        return descent_.temperature();
    }

    void on_iteration(bool accepted)
    {
        descent_.on_iteration(accepted);
        if (accepted)
            rejected_ = 0;
        else if (reheats_ < parameters().allowed_reheats
            && ++rejected_ >= parameters().rejections_before_reheat)
        {
            descent_ = initial_;
            descent_.reset();
            rejected_ = 0;
            ++reheats_;
        }
    }

    bool finished() const
    {
        return descent_.finished();
    }

    std::size_t reheats() const
    {
        return reheats_;
    }

    // [rejection-calibration]
    std::size_t calibration_samples() const
    {
        return initial_.calibration_samples();
    }

    void calibrate(std::span<const double> deltas)
    {
        initial_.calibrate(deltas);
        reset();
    }
    // [rejection-calibration]

private:
    easylocal::runners::temperature::Classic initial_;
    easylocal::runners::temperature::Classic descent_;
    std::size_t rejected_{};
    std::size_t reheats_{};
};
// [rejection-policy]

static_assert(easylocal::runners::temperature_policy<ReheatOnRejection>);
static_assert(easylocal::runners::calibrating_temperature_policy<ReheatOnRejection>);

} // namespace tutorial
