#include "SchematicLoadService.h"
#include "LoadTaskGate.h"
#include "PlacementService.h"
#include "levischematic/schematic/placement/SchematicLoader.h"
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace levischematic::app {

struct SchematicLoadService::Impl {

    struct Job {

        explicit Job(uint64_t generation) : gate(generation) {}
        LoadTaskStatus                             status;
        std::filesystem::path                      path;
        BlockPos                                   origin;
        int                                        dimension = 0;
        LoadTaskGate                               gate;
        std::optional<placement::AssetPreparation> prepared;
        std::string                                error;
    };
    mutable std::mutex                       mutex;
    std::condition_variable_any              wake;
    std::map<uint64_t, std::shared_ptr<Job>> jobs;
    std::deque<std::shared_ptr<Job>>         queued;
    std::deque<std::shared_ptr<Job>>         ready;
    std::deque<placement::AssetPreparation>  retired;
    std::deque<std::shared_ptr<Job>>         preparing;
    uint64_t                                 nextId = 1;
    std::jthread                             worker;


    Impl() : worker([this](std::stop_token stop) { run(stop); }) {}

    ~Impl() {
        worker.request_stop();
        wake.notify_all();
        worker.join();
    }

    void run(std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::shared_ptr<Job>                       job;
            std::optional<placement::AssetPreparation> garbage;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, stop, [this] { return !queued.empty() || !retired.empty(); });
                if (stop.stop_requested()) return;
                if (!retired.empty()) {
                    garbage.emplace(std::move(retired.front()));
                    retired.pop_front();
                } else {
                    job = std::move(queued.front());
                    queued.pop_front();
                }
                if (!job) {
                    lock.unlock();
                    garbage.reset();
                    continue;
                }
                if (job->gate.cancelled()) continue;
                job->status.phase = "reading";
            }
            auto             result = placement::SchematicLoader::read(job->path, job->gate.token());
            std::scoped_lock lock(mutex);
            if (job->gate.cancelled()) continue;
            if (result) job->prepared.emplace(std::move(result).value());
            else job->error = result.error().describe(job->path.string());
            job->status.phase = "preparing";
            ready.push_back(std::move(job));
        }
    }

    void retire(Job& job) {
        if (!job.prepared) return;
        std::scoped_lock lock(mutex);
        retired.push_back(std::move(*job.prepared));
        job.prepared.reset();
        wake.notify_one();
    }
};
SchematicLoadService::SchematicLoadService() : mImpl(std::make_unique<Impl>()) {}
SchematicLoadService::~SchematicLoadService() { cancelAll(); }
Result<uint64_t, std::string> SchematicLoadService::request(
    std::filesystem::path path,
    BlockPos              origin,
    int                   dimension,
    uint64_t              generation,
    unsigned              limit
) {
    std::scoped_lock lock(mImpl->mutex);
    unsigned         active = 0;
    for (auto const& [id, job] : mImpl->jobs)
        if (!job->status.terminal) ++active;
    if (active >= limit) return Result<uint64_t, std::string>::failure("Schematic loading queue is full.");

    for (auto it = mImpl->jobs.begin(); mImpl->jobs.size() >= 64 && it != mImpl->jobs.end();)
        if (it->second->status.terminal) it = mImpl->jobs.erase(it);
        else ++it;
    auto job       = std::make_shared<Impl::Job>(generation);
    job->status    = {.id = mImpl->nextId, .generation = generation, .phase = "queued"};
    job->path      = std::move(path);
    job->origin    = origin;
    job->dimension = dimension;
    mImpl->jobs.emplace(job->status.id, job);
    try {
        mImpl->queued.push_back(job);
    } catch (...) {
        mImpl->jobs.erase(job->status.id);
        throw;
    }
    ++mImpl->nextId;
    mImpl->wake.notify_one();
    return Result<uint64_t, std::string>::success(job->status.id);
}
std::optional<LoadTaskStatus> SchematicLoadService::status(uint64_t id, uint64_t generation) const {
    std::scoped_lock lock(mImpl->mutex);
    auto             it = mImpl->jobs.find(id);
    if (it == mImpl->jobs.end() || it->second->status.generation != generation) return {};
    return it->second->status;
}
bool SchematicLoadService::cancel(uint64_t id, uint64_t generation) {
    std::scoped_lock lock(mImpl->mutex);
    auto             it = mImpl->jobs.find(id);
    if (it == mImpl->jobs.end() || it->second->status.generation != generation || it->second->status.terminal)
        return false;
    if (!it->second->gate.cancel()) return false;
    it->second->status.phase    = "cancelled";
    it->second->status.terminal = true;
    return true;
}
void SchematicLoadService::cancelAll() {
    std::scoped_lock lock(mImpl->mutex);
    for (auto& [id, job] : mImpl->jobs)
        if (!job->status.terminal) {
            if (job->gate.cancel()) {
                job->status.phase    = "cancelled";
                job->status.terminal = true;
            }
        }
}
std::vector<std::string>
SchematicLoadService::tick(uint64_t generation, PlacementService& placements, util::WorkBudget const& budget) {
    std::vector<std::string> notices;
    {
        std::scoped_lock lock(mImpl->mutex);
        while (!mImpl->ready.empty()) {
            mImpl->preparing.push_back(std::move(mImpl->ready.front()));
            mImpl->ready.pop_front();
        }
    }
    while (!mImpl->preparing.empty() && budget.available()) {
        auto job = mImpl->preparing.front();
        if (job->gate.cancelled() || job->status.generation != generation) {
            (void)cancel(job->status.id, job->status.generation);
            mImpl->retire(*job);
            mImpl->preparing.pop_front();
            continue;
        }
        bool done = !job->error.empty();
        try {
            if (!done) done = placement::SchematicLoader::advance(*job->prepared, budget);
            if (done && job->error.empty()) {
                PlacementLoadInfo loaded;
                bool              published = job->gate.publish(generation, [&] {
                    loaded = placements.adoptAsset(job->prepared->asset, job->path, job->origin, job->dimension);
                });
                {
                    std::scoped_lock lock(mImpl->mutex);
                    job->status.phase = published ? "completed" : "cancelled";
                }
                job->error = published ? "Loaded '" + loaded.name + "' [ID: " + std::to_string(loaded.id)
                                             + "]; projection preparing."
                                       : "Cancelled before publication.";
            }
        } catch (std::exception const& error) {
            job->error = error.what();
            done       = true;
        }
        {
            std::scoped_lock lock(mImpl->mutex);
            if (job->prepared) {
                auto size             = job->prepared->asset->size;
                job->status.completed = job->prepared->blockCursor;
                job->status.total     = static_cast<size_t>(size.x) * size.y * size.z;
            }
            if (done) {
                if (job->status.phase != "completed" && job->status.phase != "cancelled") {
                    (void)job->gate.cancel();
                    job->status.phase = "failed";
                }
                job->status.detail   = job->error;
                job->status.terminal = true;
                notices.push_back("Load #" + std::to_string(job->status.id) + ": " + job->status.detail);
            }
        }
        if (!done) break;
        mImpl->retire(*job);
        mImpl->preparing.pop_front();
    }
    return notices;
}
} 
