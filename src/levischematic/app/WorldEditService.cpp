#include "WorldEditService.h"
#include "PlacementService.h"
#include "levischematic/editor/EditorState.h"
#include "levischematic/survival/BlockItems.h"
#include "levischematic/ui/Ui.h"
#include "levischematic/util/PositionUtils.h"
#include "levischematic/worldedit/PlayerInteraction.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/server/commands/PlayerPermissionLevel.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/levelgen/structure/StructureTemplate.h"
#include "mc/world/phys/HitResult.h"
#include <Windows.h>
#include <chrono>
#include <map>
#include <set>

namespace levischematic::app {
using namespace worldedit;
namespace {
constexpr size_t CellLimit = 1048576;
Position         key(BlockPos p) { return {p.x, p.y, p.z}; }
BlockPos         pos(Position p) { return {p[0], p[1], p[2]}; }
uint64_t         milliseconds() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count()
    );
}
Block const* air() {
    auto block = Block::tryGetFromRegistry(HashedString{"minecraft:air"});
    return block ? &*block : nullptr;
}
} 
struct WorldEditService::Impl {
    editor::EditorState& state;
    PlacementService&    placements;
    WorldEditOptions     options, request;
    Task                 task;
    PlayerInteraction    driver;
    PlayerInteraction    picker;
    struct Cell {
        Target target, before, source;
        bool   unsupported = false, hasSource = false, changed = false, alreadyMatches = false;
    };
    std::vector<Cell>           cells;
    std::set<Position>          targets;
    std::map<Position, size_t>  targetIndices;
    editor::LayerRange          layers;
    std::vector<selection::Box> boxes;
    size_t                      boxCursor = 0, cursor = 0, prepareCursor = 0, serverCursor = 0, cleanupCursor = 0;
    uint64_t                    next = 1, generation = 0;
    int                         dimension = 0;
    placement::ProjectionScan   scan;
    placement::PlacementId      owner      = 0;
    uint64_t                    revision   = 0;
    Block const*                primary    = nullptr;
    Block const*                secondary  = nullptr;
    Block const*                matchBlock = nullptr;
    size_t                      entityGaps = 0;
    bool                        submitting = false, localServerSeen = false, stopRequested = false;
    bool                        localAuthority = false, enumerated = false, captured = false;
    bool                        sourcesValidated  = false;
    size_t                      captureCursor     = 0;
    uint64_t                    verifyStarted     = 0;
    size_t                      sourceCheckCursor = 0;
    std::vector<std::string>    gaps;
    Impl(editor::EditorState& s, PlacementService& p) : state(s), placements(p) {}
    void fail(std::string text) {
        task.progress.phase  = Phase::Failed;
        task.progress.detail = std::move(text);
        
        
        stopRequested = true;
    }
    void gap(std::string text) {
        if (gaps.size() < 12) gaps.push_back(std::move(text));
    }
    Target capture(BlockSource& source, BlockPos at) { return {at, &source.getBlock(at), worldBlockItem(source, at)}; }
    bool   projection() const {
        return request.operation == Operation::Paste || request.operation == Operation::GridPaste;
    }
    bool moving() const { return request.operation == Operation::Move; }
    void
    append(BlockSource& source, BlockPos at, Block const* wanted, ItemStack item, std::optional<Target> from = {}) {
        if (!wanted) return;
        auto existing = targetIndices.find(key(at));
        if (existing != targetIndices.end()) {
            auto& cell = cells[existing->second];
            
            
            if (!projection() || !cell.target.block->isAir() || wanted->isAir()) return;
            cell.target      = {at, wanted, std::move(item)};
            cell.unsupported = cell.unsupported || !cell.target.item;
            return;
        }
        if (at.y < source.getMinHeight() || at.y >= source.getMaxHeight())
            throw std::runtime_error("Target exceeds dimension height.");
        if (request.layerOnly && !layers.containsPosition(at)) return;
        if (!source.hasChunksAt(at, 0, false)) throw std::runtime_error("Target chunk unavailable during preflight.");
        auto before = capture(source, at);
        if (projection() && !shouldWrite(request.policy, wanted->isAir(), before.block->isAir())) return;
        Cell c{
            {at, wanted, std::move(item)},
            std::move(before),
            {},
            false,
            from.has_value()
        };
        if (from) c.source = std::move(*from);
        if (c.hasSource && wanted->getBlockType().getBlockEntityType() != BlockActorType::Undefined) {
            c.target.pickSource        = c.source.position;
            c.target.sourceDataUnknown = !localAuthority;
        }
        c.unsupported = !wanted->isAir() && !c.target.item;
        if (!source.getExtraBlock(at).isAir() || (c.hasSource && !source.getExtraBlock(c.source.position).isAir())) {
            c.unsupported = true;
            gap("Secondary-layer source/target requires manual handling at " + at.toString());
        }
        if (c.unsupported) gap("No build item at " + at.toString());
        targets.insert(key(at));
        targetIndices.emplace(key(at), cells.size());
        cells.push_back(std::move(c));
        if (cells.size() > CellLimit) throw std::runtime_error("Operation exceeds 1,048,576 cells.");
    }
};
WorldEditService::WorldEditService(editor::EditorState& s, PlacementService& p) : mImpl(std::make_unique<Impl>(s, p)) {}
WorldEditService::~WorldEditService() = default;
WorldEditOptions& WorldEditService::options() { return mImpl->options; }
Progress          WorldEditService::progress() const { return mImpl->task.progress; }
bool              WorldEditService::ownsInteraction() const {
    auto phase = progress().phase;
    return phase == Phase::Running || phase == Phase::Waiting || phase == Phase::Cleaning || mImpl->driver.pending()
        || mImpl->picker.pending() || (phase == Phase::Preparing && mImpl->enumerated && !mImpl->localAuthority);
}
bool WorldEditService::submitting() const { return mImpl->submitting; }
void WorldEditService::start() { mImpl->task.start(); }
void WorldEditService::pause() { mImpl->task.pause(); }
void WorldEditService::resume() { mImpl->task.resume(); }
void WorldEditService::cancel(std::string why) {
    mImpl->task.cancel(std::move(why));
    auto client = ll::service::getClientInstance();
    auto player = client ? client->getLocalPlayer() : nullptr;
    if (player && static_cast<int>(player->getDimensionId()) == mImpl->dimension) mImpl->driver.stop(*player);
    mImpl->driver.reset();
    mImpl->picker.reset();
    mImpl->stopRequested = false;
}
Result<uint64_t, std::string> WorldEditService::prepare(IClientInstance& client, uint64_t generation) {
    using R      = Result<uint64_t, std::string>;
    auto& s      = *mImpl;
    auto  player = client.getLocalPlayer();
    if (!player || !player->isCreative()) return R::failure("Creative mode required / 需要创造模式");
    if (player->getPlayerPermissionLevel() == PlayerPermissionLevel::Visitor)
        return R::failure("Visitor cannot modify the world / 访客无世界修改权限");
    if (!s.task.progress.terminal()) return R::failure("Finish or cancel the current operation first.");
    try {
        s.request    = s.options;
        s.generation = generation;
        s.dimension  = static_cast<int>(player->getDimensionId());
        s.cells.clear();
        s.targets.clear();
        s.targetIndices.clear();
        s.layers     = s.state.view.layerRange;
        s.matchBlock = s.secondary;
        s.boxes.clear();
        s.scan = {};
        s.gaps.clear();
        s.entityGaps = 0;
        s.cursor = s.prepareCursor = s.serverCursor = s.cleanupCursor = s.boxCursor = 0;
        s.driver.reset();
        s.picker.reset();
        s.stopRequested   = false;
        s.owner           = 0;
        s.localServerSeen = false;
        s.localAuthority  = static_cast<bool>(ll::service::getServerInstance());
        s.enumerated = s.captured = false;
        s.verifyStarted           = 0;
        s.captureCursor           = 0;
        s.sourcesValidated        = false;
        s.sourceCheckCursor       = 0;
        if (s.projection()) {
            auto p = s.placements.requireSelectedPlacement();
            if (!p || p.value()->dimensionId != s.dimension || !p.value()->enabled)
                return R::failure("Select an enabled placement in this dimension.");
            s.owner    = p.value()->id;
            s.revision = p.value()->revision;
            s.placements.prepareScan(s.scan, s.owner, s.request.operation == Operation::GridPaste);
            if (s.scan.size() > CellLimit)
                return R::failure("Projection scan exceeds cell limit; use smaller regions.");
            for (auto const& leaf : s.scan.instances)
                if (leaf.asset && leaf.asset->structure)
                    s.entityGaps += leaf.asset->structure->mStructureTemplateData->mEntityData->size();
            if (s.entityGaps) s.gap("Ordinary entities retained in resource; no generic player-item NBT spawn.");
        } else {
            auto const& selection = s.state.selection;
            auto        add       = [&](auto const& a, auto const& b) {
                if (!a || !b) throw std::runtime_error("Every selected region needs two corners.");
                s.boxes.push_back({*a, *b});
            };
            if (selection.normal)
                for (auto const& region : selection.regions) add(region.corner1, region.corner2);
            else add(selection.corner1, selection.corner2);
            if (s.boxes.empty()) return R::failure("Selection is empty.");
            size_t volume = 0;
            for (auto const& b : s.boxes) {
                auto     a = b.getMin(), z = b.getMax();
                uint64_t n = 1;
                for (auto v : {int64_t(z.x) - a.x + 1, int64_t(z.y) - a.y + 1, int64_t(z.z) - a.z + 1}) {
                    if (v > int64_t(CellLimit) || n > CellLimit / v)
                        throw std::runtime_error("Selection exceeds cell limit.");
                    n *= v;
                }
                volume += static_cast<size_t>(n);
                if (volume > CellLimit) throw std::runtime_error("Selection exceeds cell limit.");
                if (s.request.operation == Operation::Clone || s.request.operation == Operation::Move) {
                    auto& source = player->getDimension().getBlockSourceFromMainChunkSource();
                    auto  actors = source.fetchEntities(
                        player,
                        AABB{
                            Vec3(a),
                            Vec3{float(z.x) + 1, float(z.y) + 1, float(z.z) + 1}
                    },
                        true,
                        false
                    );
                    for (auto actor : actors)
                        if (!actor->isPlayer()) ++s.entityGaps;
                }
            }
            if (s.entityGaps)
                s.gap("Source contains ordinary entities; no complete player-interaction clone/move for their NBT.");
            s.primary = s.state.tool.sampledBlock;
            if ((s.request.operation == Operation::Fill || s.request.operation == Operation::Replace) && !s.primary)
                return R::failure("Sample a primary block first / 请先采样目标方块");
            if (s.request.operation == Operation::Replace && !s.secondary)
                return R::failure("Sample a secondary (source match) block first.");
        }
        s.task.reset(s.next++, 0, s.moving());
        s.task.progress.phase  = Phase::Preparing;
        s.task.progress.detail = "Preflight: no world changes / 预检，不修改世界";
        return R::success(s.task.progress.id);
    } catch (std::exception const& e) {
        s.fail(e.what());
        return R::failure(e.what());
    }
}
void WorldEditService::tick(IClientInstance& client, uint64_t generation, util::WorkBudget const& budget) {
    auto& s = *mImpl;
    auto& p = s.task.progress;
    if (s.stopRequested) {
        auto player = client.getLocalPlayer();
        if (player && static_cast<int>(player->getDimensionId()) == s.dimension) s.driver.stop(*player);
        s.driver.reset();
        s.picker.reset();
        s.stopRequested = false;
    }
    if (p.terminal() || p.phase == Phase::Paused || p.phase == Phase::Ready) return;
    auto player = client.getLocalPlayer();
    if (!player || generation != s.generation || static_cast<int>(player->getDimensionId()) != s.dimension) {
        cancel("Session/dimension changed; task cancelled.");
        return;
    }
    if (!player->isCreative()) {
        s.fail("Creative mode lost; source retained.");
        return;
    }
    auto& source = player->getDimension().getBlockSourceFromMainChunkSource();
    try {
        if (s.owner) {
            auto current = s.placements.findPlacement(s.owner);
            if (!current || current->revision != s.revision) {
                cancel("Placement changed; prepare a new task.");
                return;
            }
        }
        if (p.phase == Phase::Preparing) {
            while (budget.available()) {
                if (s.projection()) {
                    if (s.prepareCursor >= s.scan.size()) break;
                    auto [leaf, expected] = s.scan.at(s.prepareCursor);
                    if (!leaf || !expected || !expected->block) {
                        ++s.prepareCursor;
                        continue;
                    }
                    if (!source.hasChunksAt(expected->world, 0, false)) {
                        p.detail = "Preflight waiting for target chunk";
                        return;
                    }
                    auto existing = s.targetIndices.find(key(expected->world));
                    if (existing == s.targetIndices.end()
                        || (s.cells[existing->second].target.block->isAir() && !expected->block->isAir())) {
                        ItemStack item;
                        try {
                            item = projectionBlockItem(*leaf, *expected, player->getDimension());
                        } catch (std::exception const& e) {
                            s.gap(e.what());
                        }
                        s.append(source, expected->world, expected->block, std::move(item));
                        if (expected->sourceIndex != SIZE_MAX && leaf->asset->structure) {
                            auto const& extra =
                                leaf->asset->structure->mStructureTemplateData->mExtraBlockIndices.get();
                            if (extra && expected->sourceIndex < extra->size()) {
                                auto index = (*extra)[expected->sourceIndex];
                                if (index >= 0
                                    && (size_t(index) >= leaf->asset->palette.size() || !leaf->asset->palette[index]
                                        || !leaf->asset->palette[index]->isAir())) {
                                    auto found = s.targetIndices.find(key(expected->world));
                                    if (found != s.targetIndices.end()) s.cells[found->second].unsupported = true;
                                    s.gap(
                                        "Projection secondary layer requires manual handling at "
                                        + expected->world.toString()
                                    );
                                }
                            }
                        }
                    }
                    ++s.prepareCursor;
                } else {
                    if (s.boxCursor >= s.boxes.size()) break;
                    auto b = s.boxes[s.boxCursor];
                    auto a = b.getMin(), z = b.getMax();
                    auto sy = uint64_t(int64_t(z.y) - a.y + 1), sz = uint64_t(int64_t(z.z) - a.z + 1);
                    auto count = uint64_t(int64_t(z.x) - a.x + 1) * sy * sz;
                    if (s.prepareCursor >= count) {
                        ++s.boxCursor;
                        s.prepareCursor = 0;
                        continue;
                    }
                    BlockPos at{
                        static_cast<int>(a.x + int64_t(s.prepareCursor / (sy * sz))),
                        static_cast<int>(a.y + int64_t((s.prepareCursor / sz) % sy)),
                        static_cast<int>(a.z + int64_t(s.prepareCursor % sz))
                    };
                    if (!source.hasChunksAt(at, 0, false)) {
                        p.detail = "Preflight waiting for source chunk";
                        return;
                    }
                    auto original = s.capture(source, at);
                    auto op       = s.request.operation;
                    if (op == Operation::Clone || op == Operation::Move) {
                        auto dest = translate(key(at), s.request.offset);
                        if (!dest) throw std::runtime_error("Destination coordinate overflow.");
                        if (!source.hasChunksAt(pos(*dest), 0, false)) {
                            p.detail = "Preflight waiting for destination chunk";
                            return;
                        }
                        s.append(source, pos(*dest), original.block, ItemStack(original.item), original);
                    } else if (
                        op != Operation::Replace
                        || (s.request.matchTypeOnly ? &original.block->getBlockType() == &s.matchBlock->getBlockType()
                                                    : original.block == s.matchBlock)
                    ) {
                        auto      wanted = op == Operation::Delete ? air() : s.primary;
                        ItemStack item;
                        if (wanted && !wanted->isAir())
                            item = copyItem(wanted->getBlockType().asItemInstance(*wanted, nullptr));
                        s.append(source, at, wanted, std::move(item));
                    }
                    ++s.prepareCursor;
                }
                p.prepared = s.cells.size();
            }
            bool finished = s.projection() ? s.prepareCursor >= s.scan.size() : s.boxCursor >= s.boxes.size();
            if (!finished) return;
            s.enumerated = true;
            if (s.localAuthority && !s.captured) {
                p.detail = "Reading authoritative preflight snapshots / 等待服务端采集快照";
                return;
            }
            if (!s.localAuthority) {
                while (s.captureCursor < s.cells.size() && budget.available()) {
                    auto& cell = s.cells[s.captureCursor];
                    if (cell.target.sourceDataUnknown && !cell.unsupported) {
                        if (ui::isOpen() || !client.getMouseGrabbed()) {
                            p.detail = "Close menu to acquire original source data / 请关闭菜单以取源方块数据";
                            return;
                        }
                        auto outcome = s.picker.acquireSourceItem(*player, cell.target, milliseconds());
                        p.detail     = s.picker.detail;
                        if (outcome == Outcome::Waiting || outcome == Outcome::InFlight) return;
                        if (outcome == Outcome::Rejected || outcome == Outcome::Unsupported) {
                            cell.unsupported = true;
                            s.gap("Original source data pick failed at " + cell.source.position.toString());
                            s.picker.reset();
                        } else cell.source.item = ItemStack(cell.target.item);
                    }
                    ++s.captureCursor;
                }
                if (s.captureCursor < s.cells.size()) return;
            }
            auto id = p.id;
            s.task.reset(id, s.cells.size(), s.moving());
            for (size_t i = 0; i < s.cells.size(); ++i) {
                if (s.cells[i].unsupported) s.task.record(i, Outcome::Unsupported);
                else if (s.cells[i].alreadyMatches) s.task.record(i, Outcome::Verified);
            }
            p.prepared = s.cells.size();
            p.detail   = "Ready / 预检完成; cells=" + std::to_string(s.cells.size())
                       + " entity gaps=" + std::to_string(s.entityGaps);
            for (auto const& gap : s.gaps) p.detail += "\n" + gap;
            return;
        }
        if (p.phase == Phase::Verifying) {
            if (!s.verifyStarted) s.verifyStarted = milliseconds();
            if (!s.localAuthority && s.moving()) {
                p.phase  = Phase::Partial;
                p.detail = "Copy observed, but authoritative remote verification unavailable; source cleanup not "
                           "started; target writes retained.";
                return;
            }
            if (s.localAuthority && milliseconds() - s.verifyStarted > 5000
                && p.verified + p.skipped + p.rejected < p.total) {
                s.fail("Server result did not match within timeout; world changes retained, Move source retained.");
                return;
            }
            if (!s.moving()) {
                if (p.verified == p.total) {
                    p.phase  = (s.entityGaps || p.skipped || p.rejected) ? Phase::Partial : Phase::Complete;
                    p.detail = "Authoritative result verified / 服务端结果已核对";
                } else p.detail = "Waiting for authoritative result / 等待服务端结果核对";
                return;
            }
            if (p.skipped || p.rejected || s.entityGaps) {
                p.phase  = Phase::Partial;
                p.detail = "Move incomplete; source cleanup not started; target writes retained.";
                return;
            }
            if (!s.task.canClean()) {
                p.detail = "Waiting for authoritative target verification; source retained. Remote NBT confirmation "
                           "unavailable.";
                return;
            }
            if (!s.sourcesValidated) {
                p.detail = "Verifying source snapshots before cleanup / 正在核对来源快照";
                return;
            }
            
            for (auto const& c : s.cells)
                if (c.hasSource && !s.targets.contains(key(c.source.position))) {
                    if (!source.hasChunksAt(c.source.position, 0, false)) {
                        p.detail = "Waiting for source chunks before cleanup";
                        return;
                    }
                    if (survival::compareBlocks(c.source.block, &source.getBlock(c.source.position))
                        != verifier::VerificationStatus::Matched) {
                        s.fail("Source changed; cleanup stopped.");
                        return;
                    }
                }
            p.phase         = Phase::Cleaning;
            s.cleanupCursor = 0;
        }
        DWORD process = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &process);
        if (process != GetCurrentProcessId() || ui::isOpen() || !client.getMouseGrabbed()
            || !client.isInWorldAndNotShowingAnyMenuScreens())
            return;
        auto now     = milliseconds();
        s.submitting = true;
        struct Reset {
            bool& flag;
            ~Reset() { flag = false; }
        } guard{s.submitting};
        if (p.phase == Phase::Cleaning) {
            while (s.cleanupCursor < s.cells.size() && budget.available()) {
                auto& c = s.cells[s.cleanupCursor];
                if (!c.hasSource || s.targets.contains(key(c.source.position)) || c.source.block->isAir()) {
                    ++s.cleanupCursor;
                    continue;
                }
                if (!s.driver.pending()
                    && survival::compareBlocks(c.source.block, &source.getBlock(c.source.position))
                           != verifier::VerificationStatus::Matched
                    && !source.getBlock(c.source.position).isAir()) {
                    s.fail("Source conflict during cleanup.");
                    return;
                }
                Target clear{c.source.position, air(), {}};
                auto   outcome = s.driver.advance(*player, clear, now);
                p.detail       = s.driver.detail;
                if (outcome == Outcome::Observed) {
                    ++s.cleanupCursor;
                    return;
                }
                if (outcome == Outcome::Rejected || outcome == Outcome::Unsupported)
                    s.fail("Source cleanup incomplete; remaining source retained.");
                return;
            }
            if (s.cleanupCursor == s.cells.size()) {
                p.phase  = Phase::Complete;
                p.detail = "Move copied and source cleanup observed; persistence verification separate.";
            }
            return;
        }
        size_t attempts = 0;
        while (budget.available() && attempts++ < std::min<size_t>(s.cells.size() + 1, 128)) {
            auto index = s.task.next();
            if (!index) break;
            auto& c = s.cells[*index];
            if (!s.driver.pending() && !c.changed
                && !blockMatches(source, c.target.position, c.before.block, c.before.item)
                && !blockMatches(source, c.target.position, c.target.block, c.target.item)) {
                s.task.record(*index, Outcome::Rejected);
                s.gap("Target changed during task");
                continue;
            }
            if (!s.driver.pending() && c.changed && !source.getBlock(c.target.position).isAir()
                && survival::compareBlocks(c.target.block, &source.getBlock(c.target.position))
                       != verifier::VerificationStatus::Matched
                && survival::compareBlocks(c.before.block, &source.getBlock(c.target.position))
                       != verifier::VerificationStatus::Matched) {
                s.task.record(*index, Outcome::Rejected);
                s.gap("Target changed between replacement interactions");
                continue;
            }
            auto outcome = s.driver.advance(*player, c.target, now);
            if (outcome == Outcome::InFlight) c.changed = true;
            s.task.record(*index, outcome);
            p.detail = s.driver.detail;
            if (outcome == Outcome::InFlight || outcome == Outcome::Observed || outcome == Outcome::Rejected) return;
        }
        if (p.phase == Phase::Complete) {
            if (s.localAuthority && p.verified + p.skipped + p.rejected < p.total) {
                p.phase  = Phase::Verifying;
                p.detail = "Waiting for authoritative result";
                return;
            }
            if (!s.localAuthority)
                for (size_t i = 0; i < s.cells.size(); ++i) {
                    auto block = s.cells[i].target.block;
                    if (block && block->getBlockType().getBlockEntityType() != BlockActorType::Undefined
                        && s.task.outcome(i) != Outcome::Verified) {
                        s.gap("Block entity data observed locally only; server NBT confirmation unavailable.");
                        break;
                    }
                }
            if (s.entityGaps || !s.gaps.empty()) p.phase = Phase::Partial;
            p.detail = "World result observed / 已观察到结果; authoritative=" + std::to_string(p.verified) + "/"
                     + std::to_string(p.total) + "; entity gaps=" + std::to_string(s.entityGaps);
            for (auto const& gap : s.gaps) p.detail += "\n" + gap;
        } else if (p.phase == Phase::Running) p.phase = Phase::Waiting;
    } catch (std::exception const& e) {
        s.fail(e.what());
    }
}
void WorldEditService::tickServer(Level& level, uint64_t generation, util::WorkBudget const& budget) {
    auto& s     = *mImpl;
    auto  phase = s.task.progress.phase;
    if (level.isClientSide() || generation != s.generation || phase == Phase::Ready || phase == Phase::Idle
        || phase == Phase::Cancelled || phase == Phase::Failed)
        return;
    auto dim = level.getDimension(s.dimension).lock();
    if (!dim) return;
    auto& source      = dim->getBlockSourceFromMainChunkSource();
    s.localServerSeen = true;
    if (phase == Phase::Preparing) {
        if (!s.enumerated || s.captured) return;
        while (s.captureCursor < s.cells.size() && budget.available()) {
            auto& c = s.cells[s.captureCursor];
            if (!source.hasChunksAt(c.target.position, 0, false)
                || (c.hasSource && !source.hasChunksAt(c.source.position, 0, false)))
                return;
            try {
                
                if (c.hasSource) {
                    c.source       = s.capture(source, c.source.position);
                    c.target.block = c.source.block;
                    c.target.item  = ItemStack(c.source.item);
                    c.unsupported  = c.unsupported || (!c.target.block->isAir() && !c.target.item);
                }
                c.alreadyMatches = blockMatches(source, c.target.position, c.target.block, c.target.item);
                ++s.captureCursor;
            } catch (std::exception const& e) {
                s.fail(e.what());
                return;
            }
        }
        s.captured = s.captureCursor == s.cells.size();
        return;
    }
    size_t visited = 0;
    while (budget.available() && visited++ < s.cells.size()) {
        auto i       = s.serverCursor++ % s.cells.size();
        auto outcome = s.task.outcome(i);
        if (outcome != Outcome::Observed && outcome != Outcome::Verified) continue;
        auto const& c = s.cells[i];
        if (!source.hasChunksAt(c.target.position, 0, false)) continue;
        try {
            if (blockMatches(source, c.target.position, c.target.block, c.target.item))
                s.task.record(i, Outcome::Verified);
            else if (outcome == Outcome::Verified) {
                s.fail("Authoritative target changed; source retained.");
                return;
            }
        } catch (std::exception const& e) {
            s.fail(e.what());
            return;
        }
    }
    if (s.moving() && s.task.canClean() && !s.sourcesValidated) {
        while (s.sourceCheckCursor < s.cells.size() && budget.available()) {
            auto const& c = s.cells[s.sourceCheckCursor];
            if (c.hasSource && !s.targets.contains(key(c.source.position))) {
                if (!source.hasChunksAt(c.source.position, 0, false)) return;
                if (!blockMatches(source, c.source.position, c.source.block, c.source.item)) {
                    s.fail("Authoritative source changed; all remaining source retained.");
                    return;
                }
            }
            ++s.sourceCheckCursor;
        }
        s.sourcesValidated = s.sourceCheckCursor == s.cells.size();
    }
}
void WorldEditService::sample(IClientInstance& client, bool secondary) {
    auto player = client.getLocalPlayer();
    if (!player) return;
    auto  start = player->getHeadPos(), dir = player->getViewVector(1);
    auto& source = player->getDimension().getBlockSourceFromMainChunkSource();
    auto  hit    = source.clip(
        start,
        start + dir * player->getPickRange(),
        false,
        ShapeType::Outline,
        16,
        false,
        false,
        player,
        [](BlockSource const&, Block const&, bool) { return true; },
        false
    );
    if (hit.mType != HitResultType::Tile) return;
    if (secondary) mImpl->secondary = &source.getBlock(hit.mBlock);
    else {
        mImpl->state.tool.sampledBlock = &source.getBlock(hit.mBlock);
        ++mImpl->state.tool.revision;
    }
}
} 
