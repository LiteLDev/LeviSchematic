#include "SelectionService.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"
#include "levischematic/selection/SelectionOperations.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/dimension/Dimension.h"

namespace levischematic::app {
void SelectionService::restoreSnapshot(selection::SelectionState snapshot) {
    cancelGrow();
    snapshot.revision      = mState.selection.revision + 1;
    snapshot.selectionMode = mState.selection.selectionMode;
    mState.selection       = std::move(snapshot);
}
bool SelectionService::startGrow(int dimension) {
    auto const& s = mState.selection;
    if (!s.corner1 || !s.corner2) return false;
    auto box = selection::Box{*s.corner1, *s.corner2};
    auto a = box.getMin(), b = box.getMax();
    mGrow = selection::GrowSelection{
        {a.x, a.y, a.z},
        {b.x, b.y, b.z}
    };
    mGrowDimension = dimension;
    mGrowRevision  = s.revision;
    mGrowStatus    = "Growing";
    return true;
}
void SelectionService::cancelGrow() {
    mGrow.reset();
    mGrowStatus = "Cancelled";
}
void SelectionService::tickGrow(Dimension& dimension, util::WorkBudget const& budget) {
    if (!mGrow) return;
    if (mGrowRevision != mState.selection.revision || static_cast<int>(dimension.getDimensionId()) != mGrowDimension) {
        cancelGrow();
        return;
    }
    auto& source = dimension.getBlockSourceFromMainChunkSource();
    while (mGrow && !mGrow->done && !mGrow->failed && budget.available())
        mGrow->step([&](auto at) -> std::optional<bool> {
            BlockPos pos{at[0], at[1], at[2]};
            if (pos.y < dimension.mHeightRange->mMin || pos.y >= dimension.mHeightRange->mMax) return false;
            if (!source.hasChunksAt(pos, 0, false)) return {};
            return !source.getBlock(pos).isAir();
        });
    mGrowStatus = "Growing; scanned=" + std::to_string(mGrow->scanned);
    if (mGrow->failed) {
        mGrow.reset();
        mGrowStatus = "Stopped: unloaded chunk or coordinate overflow";
    } else if (mGrow->done) {
        auto a = mGrow->minimum, b = mGrow->maximum;
        mGrow.reset();
        mState.selection.corner1 = BlockPos{a[0], a[1], a[2]};
        mState.selection.corner2 = BlockPos{b[0], b[1], b[2]};
        touchSelectionState();
        mGrowStatus = "Grow complete";
    }
}
void SelectionService::setNormal(bool normal) {
    auto& s = mState.selection;
    if (s.normal == normal) return;
    if (normal && s.regions.empty() && s.nextRegion == UINT32_MAX) return;
    s.normal = normal;
    if (normal && s.regions.empty()) {
        auto id = s.nextRegion++;
        s.regions.push_back({id, "Region 1", s.corner1, s.corner2});
        s.selectedRegion = id;
    } else if (normal) {
        (void)selectRegion(s.regions.front().id);
    }
    touchSelectionState();
}
bool SelectionService::addRegion(std::string name) {
    if (name.empty()) return false;
    auto& s = mState.selection;
    if (s.nextRegion == UINT32_MAX) return false;
    setNormal(true);
    if (s.nextRegion == UINT32_MAX) return false;
    if (std::any_of(s.regions.begin(), s.regions.end(), [&](auto const& r) { return r.name == name; })) return false;
    auto id = s.nextRegion++;
    s.regions.push_back({id, std::move(name), {}, {}});
    return selectRegion(id);
}
bool SelectionService::selectRegion(uint32_t id) {
    auto& s  = mState.selection;
    auto  it = std::find_if(s.regions.begin(), s.regions.end(), [&](auto const& r) { return r.id == id; });
    if (it == s.regions.end()) return false;
    s.selectedRegion = id;
    s.corner1        = it->corner1;
    s.corner2        = it->corner2;
    s.normal         = true;
    touchSelectionState();
    return true;
}
bool SelectionService::removeRegion(uint32_t id) {
    auto& s     = mState.selection;
    auto  count = std::erase_if(s.regions, [&](auto const& r) { return r.id == id; });
    if (!count) return false;
    if (s.selectedRegion == id) {
        s.selectedRegion = 0;
        s.corner1.reset();
        s.corner2.reset();
        if (!s.regions.empty()) (void)selectRegion(s.regions.front().id);
    }
    touchSelectionState();
    return true;
}
bool SelectionService::renameRegion(uint32_t id, std::string name) {
    if (name.empty()) return false;
    auto& s = mState.selection;
    if (std::any_of(s.regions.begin(), s.regions.end(), [&](auto const& r) { return r.id != id && r.name == name; }))
        return false;
    for (auto& r : s.regions)
        if (r.id == id) {
            r.name = std::move(name);
            touchSelectionState();
            return true;
        }
    return false;
}
void SelectionService::setOrigin(std::optional<BlockPos> origin) {
    mState.selection.customOrigin = origin;
    touchSelectionState();
}
bool SelectionService::resize(BlockPos amount) {
    auto box = selection::getSelectionBox(mState.selection);
    if (!box) return false;
    auto a = selection::getMinCorner(mState.selection), size = selection::getSize(mState.selection);
    auto lo = placement::checkedPosition({int64_t(a.x) - amount.x, int64_t(a.y) - amount.y, int64_t(a.z) - amount.z});
    auto hi = placement::checkedPosition(
        {int64_t(a.x) + size.x - 1 + amount.x,
         int64_t(a.y) + size.y - 1 + amount.y,
         int64_t(a.z) + size.z - 1 + amount.z}
    );
    if (!lo || !hi || lo->x > hi->x || lo->y > hi->y || lo->z > hi->z) return false;
    mState.selection.corner1 = *lo;
    mState.selection.corner2 = *hi;
    touchSelectionState();
    return true;
}
bool SelectionService::fromPlacements(
    std::vector<std::reference_wrapper<placement::PlacementInstance const>> const& list,
    placement::PlacementId                                                         id
) {
    selection::SelectionState next;
    next.normal   = true;
    next.revision = mState.selection.revision;
    for (auto const& ref : list) {
        auto const& p = ref.get();
        if ((p.parentId ? p.parentId : p.id) != id || !p.asset) continue;
        auto size = p.asset->size;
        if (static_cast<int>(p.rotation) % 2) std::swap(size.x, size.z);
        auto hi = placement::checkedPosition(
            {int64_t(p.origin.x) + size.x - 1, int64_t(p.origin.y) + size.y - 1, int64_t(p.origin.z) + size.z - 1}
        );
        if (!hi) return false;
        auto region = next.nextRegion++;
        next.regions.push_back({region, p.name + " " + std::to_string(region), p.origin, *hi});
    }
    if (next.regions.empty()) return false;
    next.customOrigin   = next.regions.front().corner1;
    next.corner1        = next.regions.front().corner1;
    next.corner2        = next.regions.front().corner2;
    next.selectedRegion = next.regions.front().id;
    mState.selection    = std::move(next);
    touchSelectionState();
    return true;
}

std::string SelectionError::describe(std::string_view target) const {
    switch (code) {
    case Code::IncompleteSelection:
        return "Selection is incomplete. Set both corners first (pos1/pos2).";
    case Code::RuntimeUnavailable:
        return detail.empty() ? "Schematic directory is not configured." : detail;
    case Code::SaveFailed:
        if (!detail.empty()) {
            return detail;
        }
        return target.empty()
            ? "Failed to save selection."
            : "Failed to save selection as '" + std::string(target) + "'.";
    }

    return "Unknown selection error.";
}

SelectionService::SelectionService(
    editor::EditorState&          state,
    RuntimeContext&               runtime,
    selection::SelectionExporter& selectionExporter
)
    : mState(state)
    , mRuntime(runtime)
    , mSelectionExporter(selectionExporter) {}

bool SelectionService::isSelectionModeEnabled() const {
    return mState.selection.selectionMode;
}

selection::SelectionOverlayView SelectionService::overlay() const {
    return selection::makeOverlayView(mState.selection);
}

void SelectionService::setSelectionCorner1(BlockPos pos) {
    mState.selection.corner1 = pos;
    touchSelectionState();
}

void SelectionService::setSelectionCorner2(BlockPos pos) {
    mState.selection.corner2 = pos;
    touchSelectionState();
}

void SelectionService::clearSelection() {
    mState.selection.corner1.reset();
    mState.selection.corner2.reset();
    mState.selection.selectedElement = selection::tool::Element::Box;
    touchSelectionState();
}

void SelectionService::setSelectionMode(bool enabled) {
    mState.selection.selectionMode = enabled;
    mState.tool.activeTool         = enabled ? editor::ToolMode::Selection : editor::ToolMode::Placement;
    ++mState.tool.revision;
    touchSelectionState();
}

void SelectionService::toggleSelectionMode() { setSelectionMode(!mState.selection.selectionMode); }

void SelectionService::selectElement(selection::tool::Element element) {
    mState.selection.selectedElement = element;
    touchSelectionState();
}
void SelectionService::cycleCornerMode() {
    using selection::tool::CornerMode;
    auto& mode = mState.selection.cornerMode;
    mode       = mode == CornerMode::Corners ? CornerMode::Expand : CornerMode::Corners;
    touchSelectionState();
}
bool SelectionService::toolClick(BlockPos pos, bool secondary) {
    if (!selection::tool::click(mState.selection, pos, secondary)) return false;
    touchSelectionState();
    return true;
}
bool SelectionService::nudgeSelected(BlockPos delta) {
    if (!selection::tool::nudge(mState.selection, delta)) return false;
    touchSelectionState();
    return true;
}

Result<SelectionSaveInfo, SelectionError> SelectionService::saveSelection(
    std::string_view name,
    Dimension&       dimension
) const {
    if (!selection::hasCompleteSelection(mState.selection)) {
        return Result<SelectionSaveInfo, SelectionError>::failure({
            .code = SelectionError::Code::IncompleteSelection,
        });
    }

    if (!mRuntime.hasSchematicDirectory()) {
        return Result<SelectionSaveInfo, SelectionError>::failure({
            .code   = SelectionError::Code::RuntimeUnavailable,
            .detail = "Schematic directory is not configured.",
        });
    }

    auto exportResult = mSelectionExporter.saveToMcstructure(
        mState.selection,
        mRuntime.schematicDirectory(),
        name,
        dimension
    );
    if (!exportResult) {
        return Result<SelectionSaveInfo, SelectionError>::failure({
            .code   = SelectionError::Code::SaveFailed,
            .path   = exportResult.error().path,
            .detail = exportResult.error().describe(name),
        });
    }

    return Result<SelectionSaveInfo, SelectionError>::success({
        .name = std::string(name),
        .size = selection::getSize(mState.selection),
        .path = exportResult.value(),
    });
}

void SelectionService::touchSelectionState() {
    auto& s=mState.selection;
    if (s.normal) for (auto& r:s.regions) if (r.id==s.selectedRegion) {
        r.corner1=s.corner1;r.corner2=s.corner2;
    }
    ++mState.selection.revision;
}

} 
