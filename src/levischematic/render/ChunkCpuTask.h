#pragma once

#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <utility>

namespace levischematic::render::detail {


class ChunkCpuTask {
public:
    ~ChunkCpuTask() { wait(); }


    [[nodiscard]] bool pending() const noexcept { return mResult.valid(); }


    [[nodiscard]] bool ready() const {
        return pending() && mResult.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
    }
    [[nodiscard]] bool running() const { return pending() && !ready(); }


    template <class Submit, class Work>
    void start(Submit&& submit, Work&& work) {
        if (pending()) throw std::logic_error("chunk CPU task already pending");
        auto task = std::make_shared<std::packaged_task<void()>>(std::forward<Work>(work));


        mResult = task->get_future();
        try {
            std::forward<Submit>(submit)(


                [task] { (*task)(); }
            );
        } catch (...) {
            mResult = {};
            throw;
        }
    }


    void take() { mResult.get(); }


    void wait() const {
        if (pending()) mResult.wait();
    }

private:
    std::future<void> mResult;
};

} 
