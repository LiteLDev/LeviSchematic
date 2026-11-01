#pragma once
#include <cstdint>
#include <mutex>
#include <stop_token>

namespace levischematic::app {

class LoadTaskGate {
public:
    
    explicit LoadTaskGate(uint64_t generation) : mGeneration(generation) {}
    
    std::stop_token token() const { return mStop.get_token(); }
    
    bool cancelled() const { return mStop.stop_requested(); }
    
    bool cancel() {
        std::scoped_lock lock(mMutex);
        if (mFinished) return false;
        mStop.request_stop();
        mFinished = true;
        return true;
    }
    
    template <class Commit>
    bool publish(uint64_t generation, Commit&& commit) {
        std::scoped_lock lock(mMutex);
        if (generation != mGeneration || mFinished || mStop.stop_requested()) return false;
        commit();
        mFinished = true;
        return true;
    }

private:
    uint64_t         mGeneration;
    std::stop_source mStop;
    std::mutex       mMutex;
    bool             mFinished = false;
};
} 
