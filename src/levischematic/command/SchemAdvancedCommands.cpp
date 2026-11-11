#include "CommandShared.h"
#include "levischematic/ui/Ui.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
namespace levischematic::command {
struct ProjectVersionParam {
    int version;
};
void registerSchemAdvancedCommands(ll::command::CommandHandle& cmd) {
    for (auto [name, page] : {
             std::pair{"world",    ui::PageId::WorldOperations},
             {"projects", ui::PageId::Projects       },
             {"signs",    ui::PageId::Signs          }
    })
        cmd.overload().text("ui").text(name).execute([page](CommandOrigin const&, CommandOutput& out) {
            auto s = acquireSession(out);
            if (s) {
                ui::requestOpen(page);
                out.success("Requested workflow page.");
            }
        });
    for (int i = 0; i < 7; ++i) {
        constexpr char const* names[]{"paste", "gridpaste", "fill", "replace", "delete", "clone", "move"};
        cmd.overload().text("world").text(names[i]).execute([i](CommandOrigin const&, CommandOutput& out) {
            auto s = acquireSession(out);
            if (!s) return;
            if (!s->worldEdit().progress().terminal()) {
                out.error("Cancel current task first.");
                return;
            }
            s->worldEdit().options().operation = static_cast<worldedit::Operation>(i);
            s->tools().setMode(static_cast<editor::ToolMode>(static_cast<int>(editor::ToolMode::Paste) + i));
            out.success("World tool selected; prepare before start.");
        });
    }
    for (auto action : {"prepare", "start", "pause", "resume", "cancel", "status"})
        cmd.overload().text("world").text(action).execute(
            [action = std::string(action)](CommandOrigin const&, CommandOutput& out) {
                auto s = acquireSession(out);
                if (!s) return;
                if (action == "prepare") {
                    auto c = ll::service::getClientInstance();
                    if (!c) {
                        out.error("Client unavailable.");
                        return;
                    }
                    auto r = s->worldEdit().prepare(*c, s->sessionGeneration());
                    if (!r) {
                        out.error(r.error());
                        return;
                    }
                } else if (action == "start") s->worldEdit().start();
                else if (action == "pause") s->worldEdit().pause();
                else if (action == "resume") s->worldEdit().resume();
                else if (action == "cancel") s->worldEdit().cancel();
                auto p = s->worldEdit().progress();
                out.success(
                    std::string(worldedit::phaseName(p.phase)) + " #" + std::to_string(p.id)
                    + " prepared=" + std::to_string(p.prepared) + " observed=" + std::to_string(p.observed) + "/"
                    + std::to_string(p.total) + " verified=" + std::to_string(p.verified) + " skipped="
                    + std::to_string(p.skipped) + " rejected=" + std::to_string(p.rejected) + "\n" + p.detail
                );
            }
        );
    cmd.overload<SchemMoveParam>().text("world").text("offset").required("dx").required("dy").required("dz").execute(
        [](CommandOrigin const&, CommandOutput& out, SchemMoveParam const& p) {
            auto s = acquireSession(out);
            if (s) {
                s->worldEdit().options().offset = {p.dx, p.dy, p.dz};
                out.success("Clone/Move offset updated for next preflight.");
            }
        }
    );
    for (auto [name, policy] : {
             std::pair{"air",    worldedit::ReplacePolicy::TargetAir   },
             {"nonair", worldedit::ReplacePolicy::SourceNonAir},
             {"all",    worldedit::ReplacePolicy::All         }
    })
        cmd.overload().text("world").text("policy").text(name).execute(
            [policy](CommandOrigin const&, CommandOutput& out) {
                auto s = acquireSession(out);
                if (s) {
                    s->worldEdit().options().policy = policy;
                    out.success("Policy updated for next preflight.");
                }
            }
        );
    for (auto [name, secondary] : {
             std::pair{"primary",   false},
             {"secondary", true }
    })
        cmd.overload().text("world").text("sample").text(name).execute(
            [secondary](CommandOrigin const&, CommandOutput& out) {
                auto s = acquireSession(out);
                auto c = ll::service::getClientInstance();
                if (s && c) {
                    s->worldEdit().sample(*c, secondary);
                    out.success("Sample requested at current aim.");
                }
            }
        );
    for (auto action : {"create", "open", "rename", "save", "work"})
        cmd.overload<SchemNamedParam>()
            .text("project")
            .text(action)
            .required("filename")
            .execute([action =
                          std::string(action)](CommandOrigin const&, CommandOutput& out, SchemNamedParam const& p) {
                auto s = acquireSession(out);
                if (!s) return;
                auto                           c = ll::service::getClientInstance();
                app::StatusResult<std::string> r = app::ok<std::string>();
                if (action == "create") {
                    if (!c || !c->getLocalPlayer()) {
                        out.error("Player unavailable.");
                        return;
                    }
                    r = s->projects().create(p.filename, BlockPos(c->getLocalPlayer()->getPosition()));
                } else if (action == "open") r = s->projects().open(p.filename);
                else if (action == "rename") r = s->projects().rename(p.filename);
                else if (action == "save") r = s->projects().saveVersion(*s, p.filename, "");
                else r = s->projects().setWork(p.filename);
                if (r) out.success("Project operation submitted.");
                else out.error(r.error());
            });
    cmd.overload<ProjectVersionParam>()
        .text("project")
        .text("checkout")
        .required("version")
        .execute([](CommandOrigin const&, CommandOutput& out, ProjectVersionParam const& p) {
            auto s = acquireSession(out);
            if (!s) return;
            auto r = s->projects().checkout(*s, p.version);
            if (r) out.success("Version projection queued; world unchanged.");
            else out.error(r.error());
        });
    for (auto action : {"status", "list", "paste", "cancel"})
        cmd.overload().text("project").text(action).execute(
            [action = std::string(action)](CommandOrigin const&, CommandOutput& out) {
                auto s = acquireSession(out);
                if (!s) return;
                if (action == "cancel") s->projects().cancel(*s);
                if (action == "paste") {
                    auto c = ll::service::getClientInstance();
                    if (!c) return;
                    auto r = s->projects().paste(*s, *c);
                    if (!r) {
                        out.error(r.error());
                        return;
                    }
                }
                if (action == "list") {
                    std::string text;
                    for (auto const& name : s->projects().list()) text += name + "\n";
                    out.success(text);
                    return;
                }
                auto p = s->projects().view();
                out.success(
                    p.name + " current=" + std::to_string(p.current) + " last pasted=" + std::to_string(p.lastPasted)
                    + " " + p.detail
                );
            }
        );
    for (auto action : {"copy", "copyback", "paste", "cancel"})
        cmd.overload().text("sign").text(action).execute(
            [action = std::string(action)](CommandOrigin const&, CommandOutput& out) {
                auto s = acquireSession(out);
                auto c = ll::service::getClientInstance();
                if (!s || !c) return;
                if (action == "cancel") {
                    s->signs().cancel();
                    out.success("Pending sign paste cancelled.");
                } else
                    out.success(
                        action == "paste" ? s->signs().arm() : s->signs().copy(*c, s->placement(), action == "copyback")
                    );
            }
        );
}
} 
