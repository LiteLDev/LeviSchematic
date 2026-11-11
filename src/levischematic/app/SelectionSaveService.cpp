#include "SelectionSaveService.h"
#include "levischematic/schematic/SchematicPathResolver.h"
#include "levischematic/schematic/placement/VolumeIndex.h"
#include "levischematic/selection/StructureCapture.h"
#include "levischematic/util/AtomicFile.h"
#include "levischematic/util/Utf8Path.h"
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
#include <future>
#include <mutex>
#include <nlohmann/json.hpp>
#include <chrono>

namespace levischematic::app {
struct SelectionSaveService::Impl {
    struct Group {
        std::unique_ptr<SelectionSaveService> child=std::make_unique<SelectionSaveService>();
        std::vector<selection::SelectionRegion> regions;
        size_t cursor=0,completed=0;
        std::filesystem::path directory;
        nlohmann::json manifest;
    };
    std::unique_ptr<Group> group;
    struct WriteControl {
        std::mutex mutex;
        bool       cancelled = false, committed = false;
    };
    SaveProgress                                 status;
    uint64_t                                     generation = 0, nextId = 1;
    int                                          dimension     = 0;
    bool                                         serverCapture = false;
    BlockPos                                     origin, size;
    SaveOptions                                  options;
    size_t                                       cursor = 0, tileCount = 0;
    std::array<int, 3>                           tiles{};
    std::unique_ptr<selection::StructureCapture> capture;
    std::future<std::string>                     writer;
    std::shared_ptr<WriteControl>                control;
    void                                         fail(std::string detail) {
        status.phase    = "失败 / Failed";
        status.detail   = std::move(detail);
        status.terminal = true;
        capture.reset();
    }
};
SelectionSaveService::SelectionSaveService() : mImpl(std::make_unique<Impl>()) {}
SelectionSaveService::~SelectionSaveService() { cancel(); }
SaveProgress SelectionSaveService::progress() const { return mImpl->status; }
void         SelectionSaveService::cancel() {
    auto& s = *mImpl;
    if (s.status.terminal) return;
    if (s.group) s.group->child->cancel();
    if (s.control) {
        std::lock_guard lock(s.control->mutex);
        if (s.control->committed) {
            s.status.phase    = "已保存 / Saved";
            s.status.terminal = true;
            return;
        }
        s.control->cancelled = true;
    }
    s.status.phase    = "已取消 / Cancelled";
    s.status.terminal = true;
    s.capture.reset();
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
        return R::failure("A save task is already active.");
    
    if (s.writer.valid()) (void)s.writer.get();
    if (s.group) {
        auto& writer=s.group->child->mImpl->writer;
        if (writer.valid() && writer.wait_for(std::chrono::seconds(0))!=std::future_status::ready)
            return R::failure("Previous region writer is finishing cancellation.");
    }
    if (directory.empty() || name.empty()) return R::failure("Schematic directory or filename is empty.");
    s.group.reset();
    if (selection.normal && !selection.regions.empty()) {
        auto group=std::make_unique<Impl::Group>();
        auto path=schematic::SchematicPathResolver(directory).makeFilePath(util::pathFromUtf8(name));
        if (!path.filename().string().ends_with(".schemset.json")) path.replace_extension(".schemset.json");
        if (std::filesystem::exists(path) && !options.overwrite) return R::failure("File exists. Confirm overwrite.");
        size_t total=0;
        std::optional<BlockPos> min;
        for (auto const& region:selection.regions) {
            if (!region.corner1 || !region.corner2) return R::failure("Every region needs both corners.");
            selection::Box box{*region.corner1,*region.corner2};
            auto a=box.getMin(),b=box.getMax();
            std::array<int64_t,3> extent{int64_t(b.x)-a.x+1,int64_t(b.y)-a.y+1,int64_t(b.z)-a.z+1};
            for (auto n:extent) if (n>INT32_MAX) return R::failure("Region extent overflow.");
            auto volume=placement::volumeSize({static_cast<int>(extent[0]),static_cast<int>(extent[1]),static_cast<int>(extent[2])});
            if (!volume || total>SIZE_MAX-*volume) return R::failure("Region volume overflow.");
            total+=*volume;
            if (!min) min=a;
            else min=BlockPos{std::min(min->x,a.x),std::min(min->y,a.y),std::min(min->z,a.z)};
        }
        auto origin=selection.customOrigin.value_or(*min);
        group->regions=selection.regions;
        group->directory=path.parent_path()/(path.stem().string()+".regions-"+
            std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directories(group->directory)) return R::failure("Cannot create generation directory.");
        group->manifest={{"version",1},{"name",name},{"origin",{origin.x,origin.y,origin.z}},
                          {"regions",nlohmann::json::array()}};
        for (size_t i=0;i<group->regions.size();++i) {
            auto const& r=group->regions[i];
            auto a=selection::Box{*r.corner1,*r.corner2}.getMin();
            auto relative=(group->directory/(std::to_string(i)+".mcstructure")).lexically_relative(path.parent_path()).generic_u8string();
            group->manifest["regions"].push_back({{"id",std::to_string(r.id)},{"name",r.name},
                {"offset",{a.x,a.y,a.z}},{"file",std::string(reinterpret_cast<char const*>(relative.data()),relative.size())}});
        }
        selection::SelectionState first;
        first.corner1=group->regions.front().corner1;first.corner2=group->regions.front().corner2;
        auto child=group->child->request(first,group->directory,"0.mcstructure",dimension,generation,{options.includeEntities,false});
        if (!child) return R::failure(child.error());
        s.dimension=dimension;s.generation=generation;s.options=options;
        s.status={s.nextId++,"采集多区域 / Capturing regions","",0,total,false,path};
        s.group=std::move(group);
        return R::success(s.status.id);
    }
    if (!selection.corner1 || !selection.corner2) return R::failure("Set both selection corners first.");
    if (directory.empty() || name.empty()) return R::failure("Schematic directory or filename is empty.");
    auto path = schematic::SchematicPathResolver(directory).makeFilePath(util::pathFromUtf8(name));
    if (path.extension() != ".mcstructure") return R::failure("Only .mcstructure is supported.");
    if (std::filesystem::exists(path) && !options.overwrite)
        return R::failure("File exists. Confirm overwrite in Save Selection.");
    auto                   a = *selection.corner1, b = *selection.corner2;
    std::array<int, 3>     size{};
    std::array<int64_t, 3> extents{
        std::abs(int64_t{a.x} - b.x) + 1,
        std::abs(int64_t{a.y} - b.y) + 1,
        std::abs(int64_t{a.z} - b.z) + 1
    };
    for (size_t i = 0; i < 3; ++i) {
        if (extents[i] > INT32_MAX) return R::failure("Selection size overflows supported coordinates.");
        size[i] = static_cast<int>(extents[i]);
    }
    auto volume = placement::volumeSize(size);
    if (!volume) return R::failure("Selection exceeds supported volume.");
    s.origin     = {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    s.size       = {size[0], size[1], size[2]};
    s.tiles      = {(size[0] - 1) / 8 + 1, (size[1] - 1) / 8 + 1, (size[2] - 1) / 8 + 1};
    s.tileCount  = size_t(s.tiles[0]) * s.tiles[1] * s.tiles[2];
    s.cursor     = 0;
    s.dimension  = dimension;
    s.generation = generation;
    s.options    = options;
    s.status     = {
        s.nextId++,
        "预检 / Checking chunks",
        "渐进采集跨越多个 tick / Captured over multiple ticks",
        0,
        *volume,
        false,
        path
    };
    s.control       = std::make_shared<Impl::WriteControl>();
    s.serverCapture = static_cast<bool>(ll::service::getServerInstance());
    return R::success(s.status.id);
}
void SelectionSaveService::tick(ClientInstance& client, uint64_t generation, util::WorkBudget const& budget) {
    auto& s = *mImpl;
    if (s.status.terminal) return;
    if (s.group) { advanceGroup(client,generation,budget);return; }
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
            else {
                s.status.phase    = "已保存 / Saved";
                s.status.terminal = true;
            }
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
    if (s.group) { s.group->child->tickServer(level,generation,budget);return; }
    if (s.status.terminal || !s.serverCapture || s.writer.valid()) return;
    if (level.isClientSide()) {
        s.fail("Local capture requires a server level tick.");
        return;
    }
    if (generation != s.generation) {
        cancel();
        return;
    }
    auto dimension = level.getDimension(s.dimension).lock();
    if (!dimension) {
        s.fail("Save dimension is unavailable.");
        return;
    }
    advance(*dimension, level, budget);
}
void SelectionSaveService::advanceGroup(ClientInstance& client,uint64_t generation,util::WorkBudget const& budget) {
    auto& s=*mImpl;
    auto& group=*s.group;
    group.child->tick(client,generation,budget);
    auto progress=group.child->progress();
    s.status.completed=group.completed+progress.completed;
    s.status.detail=std::to_string(group.cursor+1)+" / "+std::to_string(group.regions.size())+": "+progress.detail;
    if (!progress.terminal) return;
    if (progress.phase!="已保存 / Saved") {s.fail(progress.phase+": "+progress.detail);s.group.reset();return;}
    group.completed+=progress.completed;
    ++group.cursor;
    if (group.cursor==group.regions.size()) {
        try { util::writeAtomic(s.status.path,group.manifest.dump(2),s.options.overwrite);s.status.phase="已保存 / Saved";s.status.terminal=true; }
        catch (std::exception const& e) {s.fail(e.what());}
        s.group.reset();return;
    }
    selection::SelectionState next;
    next.corner1=group.regions[group.cursor].corner1;next.corner2=group.regions[group.cursor].corner2;
    auto result=group.child->request(next,group.directory,std::to_string(group.cursor)+".mcstructure",
        s.dimension,s.generation,{s.options.includeEntities,false});
    if (!result) {s.fail(result.error());s.group.reset();}
}

void SelectionSaveService::advance(Dimension& dimension, Level& level, util::WorkBudget const& budget) {
    auto& s = *mImpl;
    try {
        auto& source = dimension.getBlockSourceFromMainChunkSource();
        if (s.origin.y < dimension.mHeightRange->mMin
            || int64_t{s.origin.y} + s.size.y > dimension.mHeightRange->mMax) {
            s.fail("Selection exceeds dimension height.");
            return;
        }
        auto originAt = [&](size_t i) {
            auto pos = placement::volumePosition(i, s.tiles);
            return BlockPos{s.origin.x + pos[0] * 8, s.origin.y + pos[1] * 8, s.origin.z + pos[2] * 8};
        };
        auto check = [&](BlockPos pos) {
            if (!source.hasChunksAt(pos, 0, false)) {
                s.fail("Unloaded chunk at " + std::to_string(pos.x >> 4) + ", " + std::to_string(pos.z >> 4));
                return false;
            }
            return true;
        };
        while (!s.capture && s.cursor < s.tileCount && budget.available()) {
            if (!check(originAt(s.cursor++))) return;
        }
        if (!s.capture && s.cursor == s.tileCount && budget.available()) {
            s.capture      = std::make_unique<selection::StructureCapture>(s.origin, s.size);
            s.cursor       = 0;
            s.status.phase = "采集中 / Capturing";
        }
        while (s.capture && s.cursor < s.tileCount && budget.available()) {
            auto     origin = originAt(s.cursor);
            BlockPos offset{origin.x - s.origin.x, origin.y - s.origin.y, origin.z - s.origin.z};
            BlockPos size{
                std::min(8, s.size.x - offset.x),
                std::min(8, s.size.y - offset.y),
                std::min(8, s.size.z - offset.z)
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
        if (s.capture && s.cursor == s.tileCount && budget.available()) {
            s.status.phase = "写入中 / Writing";
            s.writer       = std::async(
                std::launch::async,
                [capture   = std::move(s.capture),
                 path      = s.status.path,
                 control   = s.control,
                 overwrite = s.options.overwrite]() {
                    try {
                        auto tag   = capture->finish();
                        auto bytes = tag->toBinaryNbt();
                        util::writeAtomicTemporary(path, bytes);
                        std::lock_guard lock(control->mutex);
                        if (control->cancelled) {
                            std::error_code ec;
                            std::filesystem::remove(util::atomicTemporaryPath(path), ec);
                            return std::string("Cancelled.");
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
    } catch (std::exception const& e) {
        s.fail(e.what());
    }
}
} 
