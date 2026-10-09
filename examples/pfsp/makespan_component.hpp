#pragma once

// The cost component of the PFSP: the makespan of the schedule.

#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/utils/input_base.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <string_view>
#include <vector>

namespace pfsp
{

// The makespan: the completion time of the last job on the last machine. A
// job starts on a machine when the machine has finished the previous job and
// the job has left the previous machine. A swap changes the completion times
// from its first position on, so there is no cheaper delta than recomputing
// them: the runners use full evaluation.
class MakespanComponent : public easylocal::input_base<PfspInstance>
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "Makespan";
    }

    using input_base::input_base;

    time_type evaluate(const Schedule& solution) const
    {
        assert(solution.order.size() == input().job_count);

        // completion[machine]: the completion time of the last scheduled job.
        std::vector<time_type> completion(input().machine_count, 0);
        for (const auto job : solution.order)
        {
            time_type previous_machine = 0;
            for (std::size_t machine = 0; machine < input().machine_count; ++machine)
            {
                completion[machine] = std::max(completion[machine], previous_machine)
                    + input().processing_time(job, machine);
                previous_machine = completion[machine];
            }
        }
        return completion.back();
    }
};

} // namespace pfsp
