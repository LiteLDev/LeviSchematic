#include "levischematic/app/SessionGate.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>

using levischematic::app::SessionGate;
using namespace std::chrono_literals;

namespace {
void check(bool condition, char const* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

SessionGate* drainingGate      = nullptr;
bool         teardownRequested = false;
int          teardownCount     = 0;
void         drain() noexcept {
    if (!teardownRequested) return;
    teardownRequested = false;
    
    auto callback = drainingGate->control();
    check(!drainingGate->acquire().owns_lock(), "closed gate rejects destructor callback business access");
    ++teardownCount;
}
} 

int main() {
    SessionGate gate(drain);
    drainingGate = &gate;
    check(!gate.acquire().owns_lock(), "starts closed");
    {
        auto control = gate.control();
        gate.publish();
    }
    auto firstGeneration = gate.generation();
    {
        
        
        
        auto lifecycle = gate.control();
        check(!gate.acquire(true).owns_lock(), "UI must not interpret a rejected render probe as no session");
        auto ui = gate.acquire();
        check(ui.owns_lock(), "UI business lease can borrow an active lifecycle scope");
        check(gate.generation() == firstGeneration, "UI admission preserves the active generation");
    }
    check(gate.acquire().owns_lock(), "UI business access remains available after lifecycle scope ends");
    {
        auto outer = gate.acquire();
        check(outer.owns_lock(), "active session admits business scope");
        check(!gate.acquire(true).owns_lock(), "recursive rendering is rejected");
        {
            auto nested = gate.acquire();
            check(nested.owns_lock(), "synchronous native listener can borrow outer scope");
            gate.close();
            auto stop         = gate.control();
            teardownRequested = true;
        }
        check(teardownCount == 0, "nested stop does not destroy borrowed services");
        auto moved = std::move(outer);
        check(!outer.owns_lock() && moved.owns_lock(), "moving lease transfers ownership");
        check(teardownCount == 0, "moved scope still pins session");
    }
    check(teardownCount == 1, "teardown runs exactly once after outer scope returns");
    {
        auto control = gate.control();
        gate.publish();
    }
    check(gate.generation() > firstGeneration, "new session advances generation");

    {
        auto busy   = gate.acquire();
        auto render = std::async(std::launch::async, [&] { return gate.acquire(true).owns_lock(); });
        check(render.wait_for(1s) == std::future_status::ready, "render does not wait behind a busy command");
        check(!render.get(), "busy render is skipped");
    }

    std::promise<void> started;
    std::future<bool>  waiter;
    {
        auto busy = gate.acquire();
        waiter    = std::async(std::launch::async, [&] {
            started.set_value();
            return gate.acquire().owns_lock();
        });
        started.get_future().wait();
        check(waiter.wait_for(20ms) == std::future_status::timeout, "business access waits for owner");
        gate.close();
        check(!gate.acquire().owns_lock(), "close rejects arrivals before waiting for owner");
    }
    check(waiter.wait_for(1s) == std::future_status::ready, "old waiter drains after owner returns");
    check(!waiter.get(), "waiting callback cannot enter closed session");

    {
        auto control = gate.control();
        gate.publish();
    }
    
    
    std::atomic_bool done  = false;
    int              state = 0;
    std::thread      reader([&] {
        while (!done.load()) {
            if (auto lease = gate.acquire(true); lease.owns_lock()) ++state;
        }
    });
    for (int i = 0; i != 1000; ++i) {
        gate.close();
        auto control = gate.control();
        state        = 0;
        gate.publish();
    }
    done = true;
    reader.join();
    check(state >= 0, "concurrent reopen stress completed");
    std::cout << "Session gate tests passed\n";
}
