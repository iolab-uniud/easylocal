#pragma once

// The cost component of the PFSP: the makespan of the schedule.

#include "instance.hpp"
#include "solution.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

namespace pfsp
{

// The makespan: the completion time of the last job on the last machine. A
// job starts on a machine when the machine has finished the previous job and
// the job has left the previous machine. A swap changes the completion times
// from its first position on, so there is no cheaper delta than recomputing
// them: the runners use full evaluation.
class MakespanComponent
{
public:
    explicit MakespanComponent(const PfspInstance& instance) : instance_{instance} {}

    time_type evaluate(const Schedule& solution) const
    {
        assert(solution.order.size() == instance_.job_count);

        // completion[machine]: the completion time of the last scheduled job.
        std::vector<time_type> completion(instance_.machine_count, 0);
        for (const auto job : solution.order)
        {
            time_type previous_machine = 0;
            for (std::size_t machine = 0; machine < instance_.machine_count; ++machine)
            {
                completion[machine] = std::max(completion[machine], previous_machine)
                    + instance_.processing_time(job, machine);
                previous_machine = completion[machine];
            }
        }
        return completion.back();
    }

private:
    const PfspInstance& instance_;
};

} // namespace pfsp
