#include "PlacementProjectionService.h"

#include "PlacementRepository.h"

#include <iterator>

namespace levishematic::placement {
namespace {

std::vector<render::ProjEntry> buildProjectionEntries(const PlacementRepository& repository) {
    std::vector<render::ProjEntry> allEntries;

    for (const auto& placement : repository.getAllPlacements()) {
        if (!placement.isEnabled() || !placement.isRenderEnabled()) {
            continue;
        }

        auto entries = placement.generateProjEntries();
        allEntries.insert(
            allEntries.end(),
            std::make_move_iterator(entries.begin()),
            std::make_move_iterator(entries.end())
        );
    }

    return allEntries;
}

} // namespace

std::shared_ptr<const render::ProjectionSnapshot>
PlacementProjectionService::rebuildProjection(const PlacementRepository& repository) const {
    return render::getProjectionState().replaceEntries(buildProjectionEntries(repository));
}

void PlacementProjectionService::rebuildAndRefresh(
    const PlacementRepository&                     repository,
    const std::shared_ptr<RenderChunkCoordinator>& coordinator
) const {
    render::getProjectionState().replaceEntriesAndTriggerRebuild(
        buildProjectionEntries(repository),
        coordinator
    );
}

void PlacementProjectionService::clearProjection() const {
    render::getProjectionState().clear();
}

} // namespace levishematic::placement
