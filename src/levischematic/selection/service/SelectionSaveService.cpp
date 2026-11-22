#include "levischematic/selection/service/SelectionSaveService.h"
#include "levischematic/schematic/convert/LitematicConverter.h"
#include "levischematic/schematic/io/LeviSchematicFile.h"
#include "levischematic/schematic/io/SchematicPathResolver.h"
#include "levischematic/schematic/io/StructureMerge.h"
#include "levischematic/schematic/placement/model/VolumeIndex.h"
#include "levischematic/selection/capture/StructureCapture.h"
#include "levischematic/shared/i18n/I18n.h"
#include "levischematic/shared/io/AtomicFile.h"
#include "levischematic/shared/io/Utf8Path.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/multiplayer/ClientLevel.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureSettings.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include <chrono>
#include <future>
#include <mutex>

namespace levischematic::app {
namespace {
enum class Format { LeviSchematic, Litematic, Mcstructure };
std::optional<Format> formatOf(std::filesystem::path const& path) {
    if (schematic::isLeviSchematicPath(path)) return Format::LeviSchematic;
    if (convert::isLitematicPath(path)) return Format::Litematic;
    if (path.extension() == ".mcstructure") return Format::Mcstructure;
    return {};
}
} // namespace

struct SelectionSaveService::Impl {
    struct Job {
        std::string id, name;
        BlockPos    min, size, offset;
    };
    struct WriteControl {
        std::mutex  mutex;
        bool        cancelled = false, committed = false;
        std::string notice;
    };
    SaveProgress                                              status;
    uint64_t                                                  generation = 0, nextId = 1;
    int                                                       dimension     = 0;
    bool                                                      serverCapture = false;
    SaveOptions                                               options;
    Format                                                    format = Format::LeviSchematic;
    std::vector<Job>                                          jobs;
    size_t                                                    job = 0;
    std::vector<std::unique_ptr<selection::StructureCapture>> finished;
    schematic::LeviSchematicWriteInfo                         info;
    size_t                                                    cursor = 0, tileCount = 0;
    std::array<int, 3>                                        tiles{};
    std::unique_ptr<selection::StructureCapture>              capture;
    std::future<std::string>                                  writer;
    std::shared_ptr<WriteControl>                             control;

    void fail(std::string detail) {
        status.phase    = tr("save.phase.failed");
        status.detail   = std::move(detail);
        status.terminal = true;
        status.saved    = false;
        capture.reset();
        finished.clear();
    }
    void succeed() {
        status.phase    = tr("save.phase.saved");
        status.detail   = control ? control->notice : std::string{};
        status.terminal = true;
        status.saved    = true;
    }
    void beginJob() {
        auto const& j = jobs[job];
        tiles         = {(j.size.x - 1) / 8 + 1, (j.size.y - 1) / 8 + 1, (j.size.z - 1) / 8 + 1};
        tileCount     = size_t(tiles[0]) * tiles[1] * tiles[2];
        cursor        = 0;
        status.phase  = tr("save.phase.checking");
        status.detail =
            jobs.size() > 1 ? tr("save.detail.region", job + 1, jobs.size(), j.name) : tr("save.detail.progressive");
    }
};

SelectionSaveService::SelectionSaveService() : mImpl(std::make_unique<Impl>()) {}
SelectionSaveService::~SelectionSaveService() { cancel(); }
SaveProgress SelectionSaveService::progress() const { return mImpl->status; }

void SelectionSaveService::cancel() {
    auto& s = *mImpl;
    if (s.status.terminal) return;
    if (s.control) {
        std::lock_guard lock(s.control->mutex);
        if (s.control->committed) {
            s.status.phase    = tr("save.phase.saved");
            s.status.terminal = true;
            s.status.saved    = true;
            return;
        }
        s.control->cancelled = true;
    }
    s.status.phase    = tr("save.phase.cancelled");
    s.status.terminal = true;
    s.capture.reset();
    s.finished.clear();
}

Result<uint64_t, std::string> SelectionSaveService::request(
    selection::SelectionState const& selection,
    std::filesystem::path            directory,
    std::string                      name,
    int                              dimension,
    uint64_t                         generation,
    SaveOptions                      options
) {
    using R = Result<uint64_t, std::string>;
    auto& s = *mImpl;
    if (!s.status.terminal
        || (s.writer.valid() && s.writer.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
        return R::failure(tr("save.error.active"));
    if (s.writer.valid()) (void)s.writer.get();
    if (directory.empty() || name.empty()) return R::failure(tr("save.error.empty_name"));
    auto path   = schematic::SchematicPathResolver(directory).makeFilePath(util::pathFromUtf8(name));
    auto format = formatOf(path);
    if (!format) return R::failure(tr("save.error.extension"));
    if (std::filesystem::exists(path) && !options.overwrite) return R::failure(tr("save.error.exists"));

    std::vector<Impl::Job> jobs;
    auto                   add = [&](std::string             id,
                                     std::string             regionName,
                                     std::optional<BlockPos> a,
                                     std::optional<BlockPos> b) -> std::optional<std::string> {
        if (!a || !b) return tr("save.error.corners");
        std::array<int64_t, 3> extent{
            std::abs(int64_t{a->x} - b->x) + 1,
            std::abs(int64_t{a->y} - b->y) + 1,
            std::abs(int64_t{a->z} - b->z) + 1
        };
        for (auto n : extent)
            if (n > INT32_MAX) return tr("save.error.overflow");
        BlockPos size{static_cast<int>(extent[0]), static_cast<int>(extent[1]), static_cast<int>(extent[2])};
        if (!placement::volumeSize({size.x, size.y, size.z})) return tr("save.error.overflow");
        jobs.push_back({
            std::move(id),
            std::move(regionName),
            {std::min(a->x, b->x), std::min(a->y, b->y), std::min(a->z, b->z)},
            size,
            {}
        });
        return {};
    };
    if (selection.normal && !selection.regions.empty()) {
        for (auto const& region : selection.regions)
            if (auto error = add(std::to_string(region.id), region.name, region.corner1, region.corner2))
                return R::failure(*error);
    } else if (auto error = add("0", path.stem().string(), selection.corner1, selection.corner2))
        return R::failure(*error);

    size_t   total = 0;
    BlockPos min   = jobs.front().min;
    for (auto const& j : jobs) {
        auto volume = *placement::volumeSize({j.size.x, j.size.y, j.size.z});
        if (total > SIZE_MAX - volume) return R::failure(tr("save.error.overflow"));
        total += volume;
        min    = {std::min(min.x, j.min.x), std::min(min.y, j.min.y), std::min(min.z, j.min.z)};
    }
    for (auto& j : jobs) j.offset = {j.min.x - min.x, j.min.y - min.y, j.min.z - min.z};

    s.info        = {};
    s.info.name   = util::utf8(path.stem());
    auto origin   = selection.customOrigin.value_or(min);
    s.info.origin = {origin.x, origin.y, origin.z};
    if (auto client = ll::service::getClientInstance(); client && client->getLocalPlayer())
        s.info.author = client->getLocalPlayer()->getRealName();

    s.jobs = std::move(jobs);
    s.job  = 0;
    s.finished.clear();
    s.capture.reset();
    s.format        = *format;
    s.dimension     = dimension;
    s.generation    = generation;
    s.options       = options;
    s.status        = {s.nextId++, "", "", 0, total, false, false, path};
    s.control       = std::make_shared<Impl::WriteControl>();
    s.serverCapture = static_cast<bool>(ll::service::getServerInstance());
    s.beginJob();
    return R::success(s.status.id);
}

void SelectionSaveService::tick(ClientInstance& client, uint64_t generation, util::WorkBudget const& budget) {
    auto& s = *mImpl;
    if (s.status.terminal) return;
    auto* player = client.getLocalPlayer();
    if (generation != s.generation || !player || static_cast<int>(player->getDimensionId()) != s.dimension) {
        cancel();
        return;
    }
    try {
        if (s.writer.valid()) {
            if (s.writer.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
            auto error = s.writer.get();
            if (!error.empty()) s.fail(error);
            else s.succeed();
            return;
        }
        if (s.serverCapture) return;
        advance(player->getDimension(), *client.getLevel(), budget);
    } catch (std::exception const& e) {
        s.fail(e.what());
    }
}

void SelectionSaveService::tickServer(Level& level, uint64_t generation, util::WorkBudget const& budget) {
    auto& s = *mImpl;
    if (s.status.terminal || !s.serverCapture || s.writer.valid()) return;
    if (level.isClientSide()) {
        s.fail(tr("save.error.server_tick"));
        return;
    }
    if (generation != s.generation) {
        cancel();
        return;
    }
    auto dimension = level.getDimension(s.dimension).lock();
    if (!dimension) {
        s.fail(tr("save.error.dimension"));
        return;
    }
    advance(*dimension, level, budget);
}

void SelectionSaveService::startWriter() {
    auto& s        = *mImpl;
    s.status.phase = tr("save.phase.writing");
    struct Part {
        std::string id, name;
        BlockPos    offset, worldMin;
    };
    std::vector<Part> parts;
    for (auto const& j : s.jobs) parts.push_back({j.id, j.name, j.offset, j.min});
    s.writer = std::async(
        std::launch::async,
        [captures  = std::move(s.finished),
         parts     = std::move(parts),
         format    = s.format,
         info      = s.info,
         path      = s.status.path,
         control   = s.control,
         overwrite = s.options.overwrite]() mutable {
            try {
                std::vector<std::unique_ptr<CompoundTag>> tags;
                for (auto& capture : captures) tags.push_back(capture->finish());
                captures.clear();
                std::string               bytes, notice;
                convert::ConversionReport report;
                switch (format) {
                case Format::LeviSchematic: {
                    std::vector<schematic::LeviSchematicRegionRef> refs;
                    for (size_t i = 0; i < parts.size(); ++i)
                        refs.push_back({parts[i].id, parts[i].name, parts[i].offset, tags[i].get()});
                    bytes = schematic::encodeLeviSchematicFile(refs, info);
                    break;
                }
                case Format::Litematic: {
                    std::vector<convert::StructureRegionRef> refs;
                    for (size_t i = 0; i < parts.size(); ++i)
                        refs.push_back({parts[i].name, parts[i].offset, tags[i].get()});
                    bytes  = convert::encodeLitematicFile(refs, info.name, report);
                    notice = report.summary();
                    break;
                }
                case Format::Mcstructure:
                    if (tags.size() == 1) bytes = tags.front()->toBinaryNbt();
                    else {
                        std::vector<schematic::MergeRegion> regions;
                        for (size_t i = 0; i < parts.size(); ++i) regions.push_back({parts[i].offset, tags[i].get()});
                        auto const& first = parts.front();
                        BlockPos    worldOrigin{
                            first.worldMin.x - first.offset.x,
                            first.worldMin.y - first.offset.y,
                            first.worldMin.z - first.offset.z
                        };
                        bytes  = schematic::mergeStructures(regions, worldOrigin)->toBinaryNbt();
                        notice = tr("save.notice.merged", parts.size());
                    }
                    break;
                }
                util::writeAtomicTemporary(path, bytes);
                std::lock_guard lock(control->mutex);
                control->notice = notice;
                if (control->cancelled) {
                    std::error_code ec;
                    std::filesystem::remove(util::atomicTemporaryPath(path), ec);
                    return tr("save.phase.cancelled");
                }
                util::commitAtomic(path, overwrite);
                control->committed = true;
                return std::string{};
            } catch (std::exception const& e) {
                std::error_code ec;
                std::filesystem::remove(util::atomicTemporaryPath(path), ec);
                return std::string(e.what());
            }
        }
    );
}

void SelectionSaveService::advance(Dimension& dimension, Level& level, util::WorkBudget const& budget) {
    auto& s = *mImpl;
    try {
        auto&       source = dimension.getBlockSourceFromMainChunkSource();
        auto const& j      = s.jobs[s.job];
        if (j.min.y < dimension.mHeightRange->mMin || int64_t{j.min.y} + j.size.y > dimension.mHeightRange->mMax) {
            s.fail(tr("save.error.height"));
            return;
        }
        auto originAt = [&](size_t i) {
            auto pos = placement::volumePosition(i, s.tiles);
            return BlockPos{j.min.x + pos[0] * 8, j.min.y + pos[1] * 8, j.min.z + pos[2] * 8};
        };
        auto check = [&](BlockPos pos) {
            if (!source.hasChunksAt(pos, 0, false)) {
                s.fail(tr("save.error.unloaded_chunk", pos.x >> 4, pos.z >> 4));
                return false;
            }
            return true;
        };
        while (!s.capture && s.cursor < s.tileCount && budget.available()) {
            if (!check(originAt(s.cursor++))) return;
        }
        if (!s.capture && s.cursor == s.tileCount && budget.available()) {
            s.capture      = std::make_unique<selection::StructureCapture>(j.min, j.size);
            s.cursor       = 0;
            s.status.phase = tr("save.phase.capturing");
        }
        while (s.capture && s.cursor < s.tileCount && budget.available()) {
            auto     origin = originAt(s.cursor);
            BlockPos offset{origin.x - j.min.x, origin.y - j.min.y, origin.z - j.min.z};
            BlockPos size{
                std::min(8, j.size.x - offset.x),
                std::min(8, j.size.y - offset.y),
                std::min(8, j.size.z - offset.z)
            };
            for (int x : {origin.x, origin.x + size.x - 1})
                for (int z : {origin.z, origin.z + size.z - 1})
                    if (!check({x, origin.y, z})) return;
            auto              tile = std::make_unique<StructureTemplate>("tile", level.getUnknownBlockTypeRegistry());
            StructureSettings settings;
            settings.mStructureOffset                           = BlockPos::ZERO();
            settings.mStructureSize                             = size;
            settings.mIgnoreEntities                            = !s.options.includeEntities;
            settings.mIgnoreBlocks                              = false;
            settings.mAllowNonTickingPlayerAndTickingAreaChunks = false;
            auto entityIds = s.options.includeEntities ? selection::captureEntityIds(source, *tile, origin, size)
                                                       : std::vector<uint64_t>{};
            tile->fillFromWorld(source, origin, settings);
            auto tileTag = tile->save();
            s.capture->append(*tileTag, offset, entityIds);
            s.status.completed += size_t(size.x) * size.y * size.z;
            ++s.cursor;
        }
        if (s.capture && s.cursor == s.tileCount) {
            s.finished.push_back(std::move(s.capture));
            if (++s.job < s.jobs.size()) s.beginJob();
            else startWriter();
        }
    } catch (std::exception const& e) {
        s.fail(e.what());
    }
}
} // namespace levischematic::app
