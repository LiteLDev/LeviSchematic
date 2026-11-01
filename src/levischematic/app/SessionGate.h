#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <utility>

namespace levischematic::app {








class SessionGate {
public:
    



    class Lock {
    public:
        


        Lock()                       = default;
        



        Lock(Lock const&)            = delete;
        



        Lock& operator=(Lock const&) = delete;
        



        Lock(Lock&& other) noexcept : mGate(std::exchange(other.mGate, nullptr)) {}
        



        Lock& operator=(Lock&& other) noexcept {
            if (this != &other) {
                release();
                mGate = std::exchange(other.mGate, nullptr);
            }
            return *this;
        }
        


        ~Lock() { release(); }
        


        [[nodiscard]] bool owns_lock() const noexcept { return mGate != nullptr; }

    private:
        friend class SessionGate;
        



        explicit Lock(SessionGate& gate) : mGate(&gate) { ++gate.mDepth; }
        


        void release() noexcept {
            if (!mGate) return;
            auto& gate = *std::exchange(mGate, nullptr);
            assert(sOwner == &gate);
            if (gate.mDepth == 1 && gate.mOnDrain) gate.mOnDrain();
            if (--gate.mDepth == 0) {
                sOwner = nullptr;
                gate.mMutex.unlock();
            }
        }
        SessionGate* mGate = nullptr;
    };

    



    explicit SessionGate(void (*onDrain)() noexcept = nullptr) : mOnDrain(onDrain) {}
    



    [[nodiscard]] Lock acquire(bool tryOnly = false) {
        if (tryOnly && sOwner == this) return {}; 
        auto const generation = mGeneration.load();
        if (!mOpen.load()) return {};
        auto lock = enter(tryOnly);
        if (!lock.owns_lock() || !mOpen.load() || generation != mGeneration.load()) return {};
        return lock;
    }
    


    [[nodiscard]] Lock control() { return enter(false); }
    


    void               close() noexcept { mOpen.store(false); }
    


    void publish() noexcept {
        assert(sOwner == this);
        ++mGeneration;
        mOpen.store(true);
    }
    


    [[nodiscard]] uint64_t generation() const noexcept { return mGeneration.load(); }
    


    [[nodiscard]] bool     isNested() const noexcept { return sOwner == this && mDepth > 1; }

private:
    



    Lock enter(bool tryOnly) {
        if (sOwner != this) {
            assert(sOwner == nullptr);
            if (tryOnly) {
                if (!mMutex.try_lock()) return {};
            } else {
                mMutex.lock();
            }
            sOwner = this;
        }
        return Lock(*this);
    }
    inline static thread_local SessionGate* sOwner = nullptr;
    std::mutex                              mMutex;
    unsigned                                mDepth = 0;
    void (*mOnDrain)() noexcept;
    std::atomic_bool     mOpen{false};
    std::atomic_uint64_t mGeneration{0};
};

} 
