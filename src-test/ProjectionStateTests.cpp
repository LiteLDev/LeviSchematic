#include "levischematic/editor/EditorState.h"
#include "levischematic/schematic/FakeWorld.h"
#include "levischematic/schematic/placement/PatchOp.h"
#include "levischematic/verifier/VerificationPollSchedule.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <stdexcept>
#include <tuple>

using namespace levischematic;

namespace {
thread_local int allocationCountdown = -1;
}


void* operator new(std::size_t size) {
    if (allocationCountdown == 0) throw std::bad_alloc();
    if (allocationCountdown > 0) --allocationCountdown;
    if (auto* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}

void operator delete(void* pointer) noexcept { std::free(pointer); }

void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }


void testPublicationRollback() {
    auto asset          = std::make_shared<placement::SchematicAsset>();
    bool reachedSuccess = false;
    int  failures       = 0;
    for (int point = 0; point < 16; ++point) {
        placement::PlacementState state;
        placement::PlacementStore store(state);
        auto                      revision = state.revision;
        allocationCountdown                = point;
        try {
            store.createPlacement(asset, {0, 0, 0}, 0, "rollback", {});
            allocationCountdown = -1;
            reachedSuccess      = true;
        } catch (std::bad_alloc const&) {
            allocationCountdown = -1;
            ++failures;
            assert(state.placements.empty() && state.order.empty() && !state.selectedId);
            assert(state.revision == revision && state.nextPlacementId == 1);
        }
        if (reachedSuccess) break;
    }
    assert(reachedSuccess && failures >= 2);
}


void testTransforms() {
    using P = placement::PlacementInstance;
    assert(render::toMinecraftMirror(P::Mirror::MIRROR_X) == ::Mirror::Z);
    assert(render::toMinecraftMirror(P::Mirror::MIRROR_Z) == ::Mirror::X);
    assert(render::toMinecraftRotation(P::Rotation::CW_90) == ::Rotation::Clockwise90);
    BlockPos size{17, 2, 3};
    for (int mirror = 0; mirror < 3; ++mirror)
        for (int rotation = 0; rotation < 4; ++rotation) {
            std::set<std::tuple<int, int, int>> seen;
            for (int x = 0; x < size.x; ++x)
                for (int y = 0; y < size.y; ++y)
                    for (int z = 0; z < size.z; ++z) {
                        auto p = placement::transformLocalPos(
                            {x, y, z},
                            size,
                            static_cast<P::Mirror>(mirror),
                            static_cast<P::Rotation>(rotation)
                        );
                        assert(p.x >= 0 && p.x < (rotation % 2 ? size.z : size.x));
                        assert(p.z >= 0 && p.z < (rotation % 2 ? size.x : size.z));
                        assert(p.y == y);
                        assert(seen.emplace(p.x, p.y, p.z).second);
                        BlockPos world{p.x - 17, p.y - 1, p.z - 16};
                        assert(util::decodePosKey(util::encodePosKey(world)) == world);
                    }
            assert(seen.size() == 102);
        }
    auto p = placement::transformLocalPos({1, 0, 2}, size, P::Mirror::MIRROR_X, P::Rotation::CW_90);
    assert((p == BlockPos{0, 0, 15}));
    assert((util::subChunkOrigin(-17, -1, 16) == BlockPos{-32, -16, 16}));
    assert(util::subChunkKeyFromWorldPos(-1, 0, 0) != util::subChunkKeyFromWorldPos(0, 0, 0));
}


void testVisibility() {
    verifier::VerifierState state;
    BlockPos                pos{-1, 64, 16};
    auto                    key   = util::WorldBlockKey{0, util::subChunkKeyFromWorldPos(pos.x, pos.y, pos.z)};
    auto&                   chunk = state.chunks[key];
    auto&                   cell  = chunk.cells[util::encodePosKey(pos)];
    cell.placementId              = 2;
    for (auto status :
         {verifier::VerificationStatus::Unknown,
          verifier::VerificationStatus::MissingBlock,
          verifier::VerificationStatus::Matched,
          verifier::VerificationStatus::BlockMismatch,
          verifier::VerificationStatus::PropertyMismatch}) {
        cell.status = status;
        bool normal =
            status == verifier::VerificationStatus::Unknown || status == verifier::VerificationStatus::MissingBlock;
        assert(state.showsProjection(0, pos, 2) == normal);
        assert(!state.showsProjection(0, pos, 1));
        assert(state.find(1, pos) == nullptr);
    }
    editor::LayerRange range{64, 64, true};
    assert(range.contains(64) && !range.contains(63) && !range.contains(65));
    state.clearing = true;
    assert(state.find(0, pos) == nullptr);
    state.clearing = false;
    chunk.cells.erase(util::encodePosKey(pos));
    assert(state.showsProjection(0, pos, 1));
}


void testOverrides() {
    placement::PlacementState state;
    placement::PlacementStore store(state);
    auto                      asset = std::make_shared<placement::SchematicAsset>();
    asset->size                     = {17, 2, 3};
    auto first                      = store.createPlacement(asset, {-17, 64, -1}, 0, "first", {});
    auto second                     = store.createPlacement(asset, {-17, 64, -1}, 0, "second", {});
    assert(state.order.back() == second && first != second);
    auto     revision = state.revision;
    BlockPos target{-1, 65, 0};
    assert(store.patchBlock(first, target, {placement::PatchOp::Kind::Remove, nullptr}));
    assert(state.revision > revision);
    assert(store.move(first, 16, 0, 0));
    assert(store.rotate(first, placement::PlacementInstance::Rotation::CW_90));
    assert(store.get(first)->overrides.contains(util::encodePosKey(target)));
    assert(store.patchBlock(first, target, {placement::PatchOp::Kind::ClearOverride, nullptr}));
    assert(!store.get(first)->overrides.contains(util::encodePosKey(target)));
    assert(store.remove(second));
    assert(state.selectedId == first);
    store.clear();
    assert(state.placements.empty() && state.order.empty() && !state.selectedId);
}


void testContentRevisions() {
    using Placement = placement::PlacementInstance;
    placement::PlacementState state;
    placement::PlacementStore store(state);
    auto                      asset = std::make_shared<placement::SchematicAsset>();
    asset->size                     = {33, 2, 17};
    auto        id                  = store.createPlacement(asset, {-17, 64, -1}, 0, "moving", {});
    auto const* placed              = store.get(id);
    auto        content             = placed->contentRevision;
    auto        revision            = placed->revision;
    assert(store.move(id, 16, -1, -32));
    assert((placed->origin == BlockPos{-1, 63, -33}));
    assert(placed->revision > revision && placed->contentRevision == content);
    assert(store.setOrigin(id, {-33, 65, 16}));
    assert(placed->contentRevision == content);
    assert(store.rotate(id, Placement::Rotation::CW_90));
    assert(placed->contentRevision > content);
    content = placed->contentRevision;
    assert(store.setMirror(id, Placement::Mirror::MIRROR_X));
    assert(placed->contentRevision > content);
    content = placed->contentRevision;
    assert(store.resetTransform(id));
    assert(placed->contentRevision > content);
    content = placed->contentRevision;
    BlockPos target{-17, 65, 16};
    assert(store.patchBlock(id, target, {placement::PatchOp::Kind::Remove, nullptr}));
    assert(placed->contentRevision > content);
    content = placed->contentRevision;
    assert(store.move(id, -16, 0, 1));
    assert(placed->contentRevision > content);
    assert(placed->overrides.contains(util::encodePosKey(target)));
    content = placed->contentRevision;
    assert(store.setOrigin(id, {16, -1, -17}));
    assert(placed->contentRevision > content);
    assert(placed->overrides.contains(util::encodePosKey(target)));
    assert(store.patchBlock(id, target, {placement::PatchOp::Kind::ClearOverride, nullptr}));
    content = placed->contentRevision;
    assert(store.move(id, -1, 1, -1));
    assert(placed->contentRevision == content);
}

void testObservationCache() {
    using verifier::VerificationCell;
    using verifier::VerificationStatus;
    alignas(Block) std::array<std::byte, sizeof(Block) * 4> storage{};
    auto*                                                   expected = reinterpret_cast<Block const*>(storage.data());
    auto*    equivalent  = reinterpret_cast<Block const*>(storage.data() + sizeof(Block));
    auto*    wrong       = reinterpret_cast<Block const*>(storage.data() + sizeof(Block) * 2);
    auto*    air         = reinterpret_cast<Block const*>(storage.data() + sizeof(Block) * 3);
    unsigned comparisons = 0;
    auto     compare     = [&](Block const* wanted, Block const* actual) {
        assert(wanted == expected);
        ++comparisons;
        if (actual == equivalent) return VerificationStatus::Matched;
        if (actual == air) return VerificationStatus::MissingBlock;
        return VerificationStatus::PropertyMismatch;
    };
    VerificationCell cell{expected, 1};
    assert(cell.observe(expected, compare));
    assert(cell.status == VerificationStatus::Matched && comparisons == 0);
    for (int repeat = 0; repeat < 1024; ++repeat) assert(!cell.observe(expected, compare));
    assert(!cell.observe(equivalent, compare));
    assert(comparisons == 1 && cell.observed == equivalent);
    for (int repeat = 0; repeat < 1024; ++repeat) assert(!cell.observe(equivalent, compare));
    assert(comparisons == 1);
    assert(cell.observe(wrong, compare));
    assert(cell.status == VerificationStatus::PropertyMismatch && comparisons == 2);
    assert(!cell.observe(wrong, compare));
    assert(cell.observe(air, compare));
    assert(cell.status == VerificationStatus::MissingBlock && comparisons == 3);
    assert(cell.observe(nullptr, compare));
    assert(cell.status == VerificationStatus::Unknown && cell.observed == nullptr);
    assert(!cell.observe(nullptr, compare));
    assert(cell.observe(air, compare));
    assert(comparisons == 4);
    assert(cell.observe(expected, compare));
    assert(cell.status == VerificationStatus::Matched && comparisons == 4);
    bool threw = false;
    try {
        cell.observe(wrong, [](Block const*, Block const*) -> VerificationStatus {
            throw std::runtime_error("compare");
        });
    } catch (std::runtime_error const&) {
        threw = true;
    }
    assert(threw && cell.observed == expected && cell.status == VerificationStatus::Matched);
    assert(cell.observe(wrong, compare));
    assert(comparisons == 5);
    auto unknown = [&](Block const*, Block const*) {
        ++comparisons;
        return VerificationStatus::Unknown;
    };
    assert(cell.observe(equivalent, unknown));
    assert(cell.status == VerificationStatus::Unknown && comparisons == 6);
    assert(!cell.observe(equivalent, unknown));
    assert(comparisons == 6);

    std::vector<VerificationCell> matched(100000, VerificationCell{expected, 1});
    for (auto& current : matched) assert(current.observe(expected, compare));
    for (auto& current : matched) assert(!current.observe(expected, compare));
    assert(comparisons == 6);
}

void testVerificationPolling() {
    using verifier::VerificationPollSchedule;
    using namespace std::chrono_literals;
    VerificationPollSchedule polling;
    auto                     start = VerificationPollSchedule::Clock::time_point{};
    polling.reset(start);
    assert(!polling.next(0, start + 1s));
    assert(!polling.next(1, start + 999ms));
    assert(polling.next(1, start + 1s) == 0);
    for (int repeat = 0; repeat < 512; ++repeat) assert(!polling.next(1, start + 1s));
    assert(!polling.next(1, start + 1999ms));
    assert(polling.next(1, start + 2s) == 0);

    polling.reset(start);
    for (size_t index = 0; index < 512; ++index) assert(polling.next(1025, start + 1s) == index);
    for (size_t index = 512; index < 1024; ++index) assert(polling.next(1025, start + 1050ms) == index);
    assert(polling.next(1025, start + 1100ms) == 1024);
    assert(!polling.next(1025, start + 1100ms));
    assert(!polling.next(1025, start + 2099ms));
    assert(polling.next(1025, start + 2100ms) == 0);
    polling.reset(start + 2100ms);
    assert(!polling.next(1, start + 3s));
    assert(polling.next(1, start + 3100ms) == 0);
}

int main() {
    testPublicationRollback();
    testTransforms();
    testVisibility();
    testOverrides();
    testContentRevisions();
    testObservationCache();
    testVerificationPolling();
    std::cout << "Projection state tests passed\n";
}
