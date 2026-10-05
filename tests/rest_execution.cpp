#include <easylocal/adapters/rest/execution.hpp>

#include <cassert>
#include <chrono>
#include <future>
#include <memory>
#include <stop_token>

namespace
{
using namespace std::chrono_literals;

void bounded_queue_rejects_excess_work_deterministically()
{
    easylocal::rest::execution_pool pool{1, 1};

    std::promise<void> first_started;
    auto first_started_future = first_started.get_future();
    std::promise<void> release_first;
    auto release_first_future = release_first.get_future().share();
    std::promise<void> first_finished;
    auto first_finished_future = first_finished.get_future();
    std::promise<void> second_finished;
    auto second_finished_future = second_finished.get_future();

    assert(pool.try_submit([
        started = std::move(first_started),
        release = std::move(release_first_future),
        finished = std::move(first_finished)]() mutable {
        started.set_value();
        release.wait();
        finished.set_value();
    }));

    assert(first_started_future.wait_for(5s) == std::future_status::ready);

    assert(pool.try_submit([
        finished = std::move(second_finished)]() mutable {
        finished.set_value();
    }));

    assert(!pool.try_submit([] {}));

    release_first.set_value();
    assert(first_finished_future.wait_for(5s) == std::future_status::ready);
    assert(second_finished_future.wait_for(5s) == std::future_status::ready);
}

void task_exceptions_do_not_kill_workers()
{
    easylocal::rest::execution_pool pool{1, 2};
    std::promise<void> survived;
    auto survived_future = survived.get_future();

    assert(pool.try_submit([] { throw 7; }));
    assert(pool.try_submit([
        survived = std::move(survived)]() mutable {
        survived.set_value();
    }));

    assert(survived_future.wait_for(5s) == std::future_status::ready);
}

void a_cancelled_task_never_runs_and_leaves_the_queue()
{
    easylocal::rest::execution_pool pool{1, 1};

    std::promise<void> first_started;
    auto first_started_future = first_started.get_future();
    std::promise<void> release_first;
    auto release_first_future = release_first.get_future().share();
    assert(pool.try_submit(
        [started = std::move(first_started),
            release = std::move(release_first_future)]() mutable {
            started.set_value();
            release.wait();
        }));
    assert(first_started_future.wait_for(5s) == std::future_status::ready);

    // Queued, then cancelled: its place goes to the next task.
    auto ran_cancelled = std::make_shared<bool>(false);
    std::stop_source cancel;
    assert(
        pool.try_submit([ran_cancelled] { *ran_cancelled = true; }, cancel.get_token()));
    cancel.request_stop();

    std::promise<void> next_finished;
    auto next_finished_future = next_finished.get_future();
    assert(pool.try_submit([finished = std::move(next_finished)]() mutable {
        finished.set_value();
    }));

    release_first.set_value();
    assert(next_finished_future.wait_for(5s) == std::future_status::ready);
    assert(!*ran_cancelled);
}

} // namespace

int main()
{
    bounded_queue_rejects_excess_work_deterministically();
    task_exceptions_do_not_kill_workers();
    a_cancelled_task_never_runs_and_leaves_the_queue();
}
