#include "levischematic/render/ChunkCpuTask.h"

#include <cstdlib>
#include <functional>
#include <iostream>
#include <latch>
#include <thread>

using levischematic::render::detail::ChunkCpuTask;
using namespace std::chrono_literals;

namespace {
void check(bool condition, char const* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void queuedAndCompletedJobsRemainExclusive() {
    ChunkCpuTask          task;
    std::function<void()> queued;
    int                   writes = 0;
    auto                  submit = [&](auto callback) { queued = std::move(callback); };
    task.start(submit, [&] { ++writes; });
    check(task.pending() && !task.ready(), "queued work immediately occupies its slot");
    check(task.running(), "queued CPU work counts against concurrency limit");
    bool rejected = false;
    try {
        task.start(submit, [&] { writes = -100; });
    } catch (std::logic_error const&) {
        rejected = true;
    }
    check(rejected && writes == 0, "second dispatch cannot overwrite queued builder work");
    queued();
    check(task.ready() && task.pending() && writes == 1, "completed work awaits render-thread consumption");
    check(!task.running(), "completed work awaiting visible publication does not occupy a CPU slot");
    rejected = false;
    try {
        task.start(submit, [] {});
    } catch (std::logic_error const&) {
        rejected = true;
    }
    check(rejected, "ready work cannot be overwritten before endRebuild");
    task.take();
    check(!task.pending(), "consuming completion releases slot");
    check(!task.running(), "consumed result no longer occupies CPU concurrency");
    task.start(submit, [&] { ++writes; });
    queued();
    task.take();
    check(writes == 2, "builder may be reused after completion consumption");
}

void runningJobDoesNotBlockPolling() {
    ChunkCpuTask task;
    std::latch   entered{1}, release{1};
    std::thread  worker;
    int          meshResult = 0;
    task.start(
        [&](auto work) { worker = std::thread(std::move(work)); },
        [&] {
            entered.count_down();
            release.wait();
            meshResult = 42;
        }
    );
    entered.wait();

    std::promise<bool> probe;
    auto               result = probe.get_future();
    std::thread        poller([&] { probe.set_value(task.ready()); });
    check(result.wait_for(2s) == std::future_status::ready, "render polling must not wait for worker");
    check(!result.get(), "running job is not ready");
    poller.join();
    release.count_down();
    task.wait();
    task.take();
    worker.join();
    check(meshResult == 42, "result consumption publishes worker writes");
}

void failureAndRejectedSubmission() {
    ChunkCpuTask task;
    bool         caught = false;
    try {
        task.start([](auto) { throw std::runtime_error("executor rejected"); }, [] {});
    } catch (std::runtime_error const&) {
        caught = true;
    }
    check(caught && !task.pending(), "rejected submission leaves no ghost slot");
    task.start(
        [&](auto work) {
            check(task.pending(), "slot exists even before an inline executor runs");
            work();
        },
        [] { throw std::runtime_error("build failed"); }
    );
    check(task.ready(), "worker exception signals completion");
    caught = false;
    try {
        task.take();
    } catch (std::runtime_error const&) {
        caught = true;
    }
    check(caught && !task.pending(), "failed build is observable and releases its slot");
}

void destructionDrainsWorker() {
    auto        task = std::make_unique<ChunkCpuTask>();
    std::latch  entered{1}, release{1};
    std::thread worker;
    int         borrowedResource = 0;
    task->start(
        [&](auto work) { worker = std::thread(std::move(work)); },
        [&] {
            entered.count_down();
            release.wait();
            borrowedResource = 7;
        }
    );
    entered.wait();
    std::promise<void> starting, destroyed;
    auto               started = starting.get_future();
    auto               done    = destroyed.get_future();
    std::thread        teardown([owned = std::move(task), &starting, &destroyed]() mutable {
        starting.set_value();
        owned.reset();
        destroyed.set_value();
    });
    started.wait();
    check(done.wait_for(50ms) == std::future_status::timeout, "destruction waits for borrowed resources");
    release.count_down();
    check(done.wait_for(2s) == std::future_status::ready, "drain requires no main-thread completion callback");
    teardown.join();
    worker.join();
    check(borrowedResource == 7, "worker completes before teardown returns");
}
} 

int main() {
    queuedAndCompletedJobsRemainExclusive();
    runningJobDoesNotBlockPolling();
    failureAndRejectedSubmission();
    destructionDrainsWorker();
    std::cout << "ChunkCpuTask lifecycle regression tests passed\n";
}
