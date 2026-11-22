#include "levischematic/schematic/convert/ConversionService.h"
#include "levischematic/schematic/convert/LitematicConverter.h"
#include <mutex>
#include <thread>

namespace levischematic::convert {
struct ConversionService::Impl {
    mutable std::mutex              mutex;
    std::optional<ConversionStatus> status;
    uint64_t                        nextId = 1;
    std::jthread                    worker;
};
ConversionService::ConversionService() : mImpl(std::make_unique<Impl>()) {}
ConversionService::~ConversionService() { shutdown(); }

app::Result<uint64_t, std::string>
ConversionService::request(std::filesystem::path source, std::filesystem::path target, bool overwrite) {
    using R = app::Result<uint64_t, std::string>;
    std::unique_lock lock(mImpl->mutex);
    if (mImpl->status && !mImpl->status->terminal) return R::failure("A conversion is already running.");
    if (!std::filesystem::exists(source)) return R::failure("Source file not found: " + source.string());
    lock.unlock();
    if (mImpl->worker.joinable()) mImpl->worker.join();
    lock.lock();
    auto id       = mImpl->nextId++;
    mImpl->status = ConversionStatus{id, source, target, "running", "", false};
    mImpl->worker = std::jthread([this, id, source, target, overwrite](std::stop_token stop) {
        ConversionReport report;
        std::string      phase = "completed", detail;
        try {
            convertFile(source, target, overwrite, stop, report);
            detail = report.summary();
        } catch (std::exception const& e) {
            phase  = stop.stop_requested() ? "cancelled" : "failed";
            detail = e.what();
        }
        std::scoped_lock guard(mImpl->mutex);
        if (mImpl->status && mImpl->status->id == id) {
            mImpl->status->phase    = phase;
            mImpl->status->detail   = detail;
            mImpl->status->terminal = true;
        }
    });
    return R::success(id);
}
std::optional<ConversionStatus> ConversionService::status() const {
    std::scoped_lock lock(mImpl->mutex);
    return mImpl->status;
}
bool ConversionService::cancel() {
    std::scoped_lock lock(mImpl->mutex);
    if (!mImpl->status || mImpl->status->terminal) return false;
    return mImpl->worker.request_stop();
}
void ConversionService::shutdown() {
    if (!mImpl) return;
    mImpl->worker.request_stop();
    if (mImpl->worker.joinable()) mImpl->worker.join();
}
} // namespace levischematic::convert
