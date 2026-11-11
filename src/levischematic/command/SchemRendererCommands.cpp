#include "levischematic/command/CommandShared.h"

#include "levischematic/app/AppKernel.h"
#include "levischematic/app/ConfigStore.h"
#include "levischematic/render/SchematicChunkRenderer.h"
#include "levischematic/render/ActorProjection.h"
#include "levischematic/LeviSchematic.h"
#include <nlohmann/json.hpp>
#include <fstream>

#include <string>

namespace levischematic::command {



void registerSchemRendererCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload().text("renderer").text("stats").execute(
    




    [&](CommandOrigin const&, CommandOutput& output) {
        auto session = acquireSession(output);
        if (!session) return;
        auto stats = session->schematicRenderer().debugStats();
        auto actors = session->actors().stats();

        std::string message = "Schematic preview renderer entries: " + std::to_string(stats.size()) + "\n";
        nlohmann::json coverage = nlohmann::json::array();
        for (auto const& item : stats) {
            message += "  #" + std::to_string(item.placementId) + ": dimension=" + std::to_string(item.dimensionId)
                     + ", revision=" + std::to_string(item.revision) + ", renderTick=" + std::to_string(item.renderTick)
                     + ", world=" + (item.hasWorld ? "yes" : "no") + ", builder="
                     + (item.hasChunkBuilder ? "yes" : "no") + ", sorter=" + (item.hasChunkSorter ? "yes" : "no")
                     + ", cacheReady=" + (item.instanceCacheReady ? "yes" : "no")
                     + ", instances=" + std::to_string(item.chunkInstanceCount)
                     + ", sortedLastFrame=" + std::to_string(item.lastSortedInstanceCount)
                     + ", layersSubmittedLastFrame=" + std::to_string(item.lastSubmittedLayerCount)
                     + ", coveredChunks=" + std::to_string(item.submittedOrigins.size()) + "/"
                     + std::to_string(item.expectedOrigins.size()) + ", buildFailed=" + (item.buildFailed ? "yes" : "no")
                     + ", blockEntities=" + std::to_string(item.blockEntities.submitted)
                     + ", highlighted=" + std::to_string(item.blockEntities.highlighted)
                     + ", forwardVariations=" + std::to_string(item.blockEntities.forwardVariations)
                     + ", entitySkip=" + (item.blockEntities.skipped.empty() ? "none" : item.blockEntities.skipped)
                     + "\n";
            coverage.push_back({{"placementId", item.placementId}, {"renderTick", item.renderTick},
                {"buildFailed", item.buildFailed}, {"pendingBuildCount", item.pendingBuildCount},
                {"expectedOrigins", item.expectedOrigins}, {"submittedOrigins", item.submittedOrigins},
                {"globalActors", {{"loaded", actors.loaded}, {"submitted", actors.submitted},
                    {"skipped", actors.skipped}, {"detail", actors.detail},
                    {"submittedByType", actors.submittedByType},
                    {"missingRendererByType", actors.missingRendererByType},
                    {"legacyByType", actors.legacyByType}}}});
        }
        if (app::configStore().snapshot().diagnostics) {
            try {
                auto dir = LeviSchematic::getInstance().getSelf().getDataDir() / "diagnostics";
                std::filesystem::create_directories(dir);
                std::ofstream out(dir / "renderer-stats.json", std::ios::binary);
                out << coverage.dump(2) << '\n';
                if (!out) throw std::runtime_error("Cannot write renderer coverage.");
            } catch (std::exception const& e) { output.error(e.what()); }
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
