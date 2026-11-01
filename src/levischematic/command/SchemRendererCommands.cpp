#include "levischematic/command/CommandShared.h"

#include "levischematic/app/AppKernel.h"
#include "levischematic/render/SchematicChunkRenderer.h"

#include <string>

namespace levischematic::command {



void registerSchemRendererCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload().text("renderer").text("stats").execute(
    




    [&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto stats = session->schematicRenderer().debugStats();

        std::string message = "Schematic preview renderer entries: " + std::to_string(stats.size()) + "\n";
        for (auto const& item : stats) {
            message += "  #" + std::to_string(item.placementId) + ": dimension=" + std::to_string(item.dimensionId)
                     + ", revision=" + std::to_string(item.revision) + ", renderTick=" + std::to_string(item.renderTick)
                     + ", world=" + (item.hasWorld ? "yes" : "no") + ", builder="
                     + (item.hasChunkBuilder ? "yes" : "no") + ", sorter=" + (item.hasChunkSorter ? "yes" : "no")
                     + ", cacheReady=" + (item.instanceCacheReady ? "yes" : "no")
                     + ", instances=" + std::to_string(item.chunkInstanceCount)
                     + ", sortedLastFrame=" + std::to_string(item.lastSortedInstanceCount)
                     + ", layersSubmittedLastFrame=" + std::to_string(item.lastSubmittedLayerCount) + "\n";
        }
        output.success(message);
    });

    schemCmd.overload().text("renderer").text("reset").execute(
    




    [&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        session->resetRenderer();
        output.success(
            "Schematic preview runtime reset. Placements retained; resources rebuild on the next valid frame."
        );
    });
}

} 
