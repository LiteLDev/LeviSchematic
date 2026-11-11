#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
namespace levischematic::worldedit {

struct DataPickReceipt {
    uint64_t sequence = 0;
    int      slot     = -1;
};
class DataPick {
    inline static std::mutex      mMutex;
    inline static DataPickReceipt mReceipt;

public:
    inline static std::atomic_bool available{false};
    static DataPickReceipt         receipt() {
        std::lock_guard lock(mMutex);
        return mReceipt;
    }
    static void received(int slot) {
        std::lock_guard lock(mMutex);
        ++mReceipt.sequence;
        mReceipt.slot = slot;
    }
};
} 
