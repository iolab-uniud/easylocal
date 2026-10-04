#pragma once

// The PFSP Input: the processing time of each job on each machine.

#include <cassert>
#include <cstddef>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pfsp
{

using job_id = std::size_t;
using time_type = int;

// A Permutation Flowshop: every job is processed on the machines in the same
// order, and the jobs in the same order on every machine. The text format is
// Taillard's matrix: the number of jobs and of machines, then one row per
// machine with the processing time of each job.
struct PfspInstance
{
    std::size_t job_count{};
    std::size_t machine_count{};
    // processing[machine * job_count + job]
    std::vector<time_type> processing;

    static PfspInstance read(std::istream& input)
    {
        std::size_t jobs{};
        std::size_t machines{};
        if (!(input >> jobs >> machines) || jobs < 2 || machines < 1)
            throw std::runtime_error("invalid PFSP instance header");

        PfspInstance instance{
            .job_count = jobs,
            .machine_count = machines,
            .processing = std::vector<time_type>(jobs * machines),
        };
        for (auto& time : instance.processing)
            if (!(input >> time) || time < 0)
                throw std::runtime_error("invalid PFSP processing time");
        return instance;
    }

    time_type processing_time(job_id job, std::size_t machine) const
    {
        assert(job < job_count);
        assert(machine < machine_count);
        return processing[machine * job_count + job];
    }

    std::string describe() const
    {
        return "jobs=" + std::to_string(job_count)
            + ", machines=" + std::to_string(machine_count);
    }
};

} // namespace pfsp
