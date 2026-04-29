#include "levischematic/command/CommandShared.h"

namespace levischematic::command {

void registerSchemViewCommands(ll::command::CommandHandle& schemCmd) {
    schemCmd.overload<SchemViewYValueParam>()
        .text("view")
        .text("y")
        .text("min")
        .required("value")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemViewYValueParam const& param) {
            auto& viewService = app::getAppKernel().view();
            auto  result      = viewService.setMinY(param.value);
            if (!result) {
                output.error(result.error().describe());
                return;
            }

            refreshProjectionStateForOrigin(origin);
            auto const& range = viewService.currentLayerRange();
            output.success("Layer range enabled: Y {}..{}", range.minY, range.maxY);
        });

    schemCmd.overload<SchemViewYValueParam>()
        .text("view")
        .text("y")
        .text("max")
        .required("value")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemViewYValueParam const& param) {
            auto& viewService = app::getAppKernel().view();
            auto  result      = viewService.setMaxY(param.value);
            if (!result) {
                output.error(result.error().describe());
                return;
            }

            refreshProjectionStateForOrigin(origin);
            auto const& range = viewService.currentLayerRange();
            output.success("Layer range enabled: Y {}..{}", range.minY, range.maxY);
        });

    schemCmd.overload<SchemViewYRangeParam>()
        .text("view")
        .text("y")
        .text("range")
        .required("minY")
        .required("maxY")
        .execute([&](CommandOrigin const& origin,
                     CommandOutput&      output,
                     SchemViewYRangeParam const& param) {
            auto& viewService = app::getAppKernel().view();
            auto  result      = viewService.setRange(param.minY, param.maxY);
            if (!result) {
                output.error(result.error().describe());
                return;
            }

            refreshProjectionStateForOrigin(origin);
            output.success("Layer range enabled: Y {}..{}", param.minY, param.maxY);
        });

    schemCmd.overload()
        .text("view")
        .text("y")
        .text("enable")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto& viewService = app::getAppKernel().view();
            (void)viewService.enableLayerRange();
            refreshProjectionStateForOrigin(origin);

            auto const& range = viewService.currentLayerRange();
            output.success("Layer range enabled: Y {}..{}", range.minY, range.maxY);
        });

    schemCmd.overload()
        .text("view")
        .text("y")
        .text("disable")
        .execute([&](CommandOrigin const& origin, CommandOutput& output) {
            auto& viewService = app::getAppKernel().view();
            (void)viewService.disableLayerRange();
            refreshProjectionStateForOrigin(origin);

            auto const& range = viewService.currentLayerRange();
            output.success("Layer range disabled. Stored range: Y {}..{}", range.minY, range.maxY);
        });

    schemCmd.overload()
        .text("view")
        .text("y")
        .text("info")
        .execute([&](CommandOrigin const&, CommandOutput& output) {
            auto const& range = app::getAppKernel().view().currentLayerRange();

            std::string msg = "View Layer Range:\n";
            msg += "  Enabled: " + std::string(range.enabled ? "Yes" : "No") + "\n";
            msg += "  Min Y: " + std::to_string(range.minY) + "\n";
            msg += "  Max Y: " + std::to_string(range.maxY) + "\n";

            output.success(msg);
        });
}

} // namespace levischematic::command
