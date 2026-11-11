#include "SessionPersistence.h"
#include "levischematic/schematic/placement/PlacementGeometry.h"
#include "levischematic/schematic/placement/SchematicContent.h"
#include "levischematic/schematic/placement/SchematicLoader.h"
#include "levischematic/schematic/placement/SchematicSetIO.h"
#include "levischematic/session/SessionDocument.h"
#include "levischematic/session/WorldIdentity.h"
#include "levischematic/util/AtomicFile.h"
#include "levischematic/util/Utf8Path.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/ServerInfo.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/network/GameConnectionInfo.h"
#include "mc/world/level/block/Block.h"
#include <chrono>
#include <fstream>
#include <future>
#include <stop_token>
#include <unordered_set>

namespace levischematic::app {
namespace {
using session::Json;
Json     position(BlockPos const& p) { return Json::array({p.x, p.y, p.z}); }
BlockPos readPosition(Json const& value) {
    auto p = session::decodePosition(value);
    return {p[0], p[1], p[2]};
}
std::string utf8Path(std::filesystem::path const& path) {
    auto value = path.generic_u8string();
    return {reinterpret_cast<char const*>(value.data()), value.size()};
}
Json resourceRequests(Json const& document) {
    auto requests=document.at("placements");
    if (document.contains("resources")) for(auto const& r:document.at("resources"))requests.push_back(r);
    return requests;
}
Json captureDimension(editor::EditorState const& state, int dimension) {
    Json result{
        {"placements",   Json::array()                                                               },
        {"selected",     nullptr                                                                     },
        {"corner1",      state.selection.corner1 ? position(*state.selection.corner1) : Json(nullptr)},
        {"corner2",      state.selection.corner2 ? position(*state.selection.corner2) : Json(nullptr)},
        {"corner_mode",  static_cast<int>(state.selection.cornerMode)                                },
        {"element",      static_cast<int>(state.selection.selectedElement)                           },
        {"tool",
         static_cast<int>(
             editor::creativeTool(state.tool.activeTool)
                 ? (editor::areaTool(state.tool.activeTool) ? editor::ToolMode::Selection : editor::ToolMode::Placement)
                 : state.tool.activeTool
         )                                                                                           },
        {"tool_enabled", state.tool.enabled                                                          },
        {"layers",       session::encodeLayer(state.view.layerRange)                                 }
    };
    if (editor::creativeTool(state.tool.activeTool)) result["creative_tool"] = static_cast<int>(state.tool.activeTool);
    result["drafts"]=Json::object();
    auto const& selection=state.selection;
    result["normal"]=selection.normal;result["selected_region"]=selection.selectedRegion;
    result["next_region"]=selection.nextRegion;
    result["selection_origin"]=selection.customOrigin?position(*selection.customOrigin):Json(nullptr);
    result["selection_regions"]=Json::array();
    for (auto const& r:selection.regions) result["selection_regions"].push_back({{"id",r.id},{"name",r.name},
        {"corner1",r.corner1?position(*r.corner1):Json(nullptr)}, {"corner2",r.corner2?position(*r.corner2):Json(nullptr)}});
    for (auto id : state.placements.order) {
        auto const& p = state.placements.placements.at(id);
        if (p.dimensionId != dimension) continue;
        if (state.placements.selectedId == id) result["selected"] = result["placements"].size();
        Json value{
            {"path",      utf8Path(p.filePath)        },
            {"name",      p.name                      },
            {"origin",    position(p.origin)          },
            {"rotation",  static_cast<int>(p.rotation)},
            {"mirror",    static_cast<int>(p.mirror)  },
            {"enabled",   p.enabled                   },
            {"render",    p.renderEnabled             },
            {"overrides", Json::array()               },
            {"local_overrides", true},
            {"locked", p.locked}, {"axis_locks", p.axisLocked}, {"entities", p.entitiesEnabled}
        };
        value["grid"]={{"enabled",p.grid.enabled},{"spacing",p.grid.spacing},{"negative",p.grid.negative},{"positive",p.grid.positive}};
        value["regions"]=Json::object();
        for (auto const& [id,settings]:p.regions) value["regions"][id]={{"enabled",settings.enabled},
            {"offset",position(settings.offset)},{"rotation",settings.rotation},{"mirror",settings.mirror}};
        if (p.asset && p.asset->modified) {
            auto key=utf8Path(p.filePath);
            if (!result["drafts"].contains(key)) result["drafts"][key]=placement::encodeDraft(*p.asset);
            value["draft_ref"]=key;
        }
        for (auto const& [key, entry] : p.overrides) {
            bool remove = entry.kind == placement::OverrideEntry::Kind::Remove;
            if (!remove && !entry.block) throw std::runtime_error("Cannot persist unresolved block override.");
            value["overrides"].push_back({
                {"position", position(util::decodePosKey(key))                    },
                {"remove",   remove                                               },
                {"block",    remove ? "" : entry.block->mSerializationId->toSnbt()}
            });
        }
        result["placements"].push_back(std::move(value));
    }
    result["resources"]=Json::array();
    std::unordered_set<std::string> placed;
    for(auto const& p:result.at("placements"))placed.insert(p.at("path").get<std::string>());
    for(auto const& [id,r]:state.resources.records) {
        auto path=utf8Path(r.path);
        if(placed.contains(path))continue;
        Json record{{"path",path}};
        if(r.content->modified) {
            if(!result["drafts"].contains(path))result["drafts"][path]=placement::encodeDraft(*r.content);
            record["draft_ref"]=path;
        }
        result["resources"].push_back(std::move(record));
    }
    return result;
}
} 
struct SessionPersistence::Impl {
    editor::EditorState&                  state;
    placement::PlacementStore&            store;
    std::filesystem::path                 directory, file;
    SessionPersistenceStatus              status;
    Json                                  document;
    int                                   dimension = INT32_MIN;
    std::unordered_set<int>               pending;
    std::unordered_map<int, Json>         live;
    std::array<uint64_t, 4>               revisions{UINT64_MAX, 0, 0, 0};
    std::array<uint64_t, 4>               restoreRevisions{};
    bool                                  dirty = false, initialized = false;
    std::chrono::steady_clock::time_point changed;
    struct Restored {
        std::vector<placement::AssetPreparation> assets;
        std::string                              error;
    };
    std::future<Restored>    reader;
    std::future<std::string> writer;
    std::stop_source         stop;
    Json                     restoreDocument;
    size_t                   prepareCursor = 0;
    std::optional<Restored>  preparing;
    Impl(editor::EditorState& s, placement::PlacementStore& p, std::filesystem::path path)
    : state(s),
      store(p),
      directory(std::move(path)) {}
    void capture() {
        if (!status.available || dimension == INT32_MIN || pending.contains(dimension) || status.restoring) return;
        auto value = captureDimension(state, dimension);
        live.insert_or_assign(dimension, value);
        document["dimensions"][std::to_string(dimension)] = std::move(value);
        for (auto& [other, snapshot] : live) {
            if (other == dimension || pending.contains(other)) continue;
            auto updated           = captureDimension(state, other);
            snapshot["placements"] = std::move(updated["placements"]);
              snapshot["drafts"] = std::move(updated["drafts"]);
              snapshot["resources"] = std::move(updated["resources"]);
            if (!updated["selected"].is_null()) snapshot["selected"] = updated["selected"];
            if (!snapshot["selected"].is_null() && snapshot["selected"].get<size_t>() >= snapshot["placements"].size())
                snapshot["selected"] = nullptr;
            document["dimensions"][std::to_string(other)] = snapshot;
        }
        if (!dirty) changed = std::chrono::steady_clock::now();
        dirty = true;
    }
    void restoreView(Json const& value) {
        auto first = value.at("corner1"), second = value.at("corner2");
        auto cornerMode = session::decodeInteger(value.at("corner_mode")),
             element    = session::decodeInteger(value.at("element"));
        int  tool       = session::decodeInteger(value.at("tool"));
        if (value.contains("creative_tool")) {
            auto mode = session::decodeInteger(value.at("creative_tool"));
            if (mode < static_cast<int>(editor::ToolMode::Paste) || mode > static_cast<int>(editor::ToolMode::Move))
                throw std::runtime_error("Invalid saved creative tool.");
            tool = mode;
        }
        if (cornerMode < 0 || cornerMode > 1 || element < 0 || element > 3 || tool < 0
            || tool > static_cast<int>(editor::ToolMode::Move))
            throw std::runtime_error("Invalid saved selection/tool mode.");
        auto layers               = session::decodeLayer(value.at("layers"));
        auto selection            = state.selection;
        selection.corner1         = first.is_null() ? std::optional<BlockPos>{} : readPosition(first);
        selection.corner2         = second.is_null() ? std::optional<BlockPos>{} : readPosition(second);
        selection.cornerMode      = static_cast<selection::tool::CornerMode>(cornerMode);
        selection.selectedElement = static_cast<selection::tool::Element>(element);
        selection.selectionMode   = editor::areaTool(static_cast<editor::ToolMode>(tool));
        selection.normal=value.value("normal",false);
        selection.selectedRegion=value.value("selected_region",uint32_t{0});
        selection.nextRegion=value.value("next_region",uint32_t{1});
        selection.regions.clear();selection.customOrigin.reset();
        if (value.contains("selection_origin") && !value.at("selection_origin").is_null())
            selection.customOrigin=readPosition(value.at("selection_origin"));
        if (value.contains("selection_regions")) {
            std::unordered_set<uint32_t> ids;
            for (auto const& r:value.at("selection_regions")) {
                uint32_t id=r.at("id");
                if (!id || id>=selection.nextRegion || !ids.insert(id).second) throw std::runtime_error("Invalid saved region ID.");
                selection.regions.push_back({id,r.at("name"),r.at("corner1").is_null()?std::optional<BlockPos>{}:readPosition(r.at("corner1")),
                    r.at("corner2").is_null()?std::optional<BlockPos>{}:readPosition(r.at("corner2"))});
            }
            if (selection.selectedRegion && !ids.contains(selection.selectedRegion)) throw std::runtime_error("Invalid selected region.");
        }
        ++selection.revision;
        bool enabled          = value.at("tool_enabled").get<bool>();
        state.selection       = selection;
        state.tool.activeTool = static_cast<editor::ToolMode>(tool);
        state.tool.enabled    = enabled;
        ++state.tool.revision;
        state.view.layerRange = layers;
        ++state.view.revision;
    }
    void publish() {
        std::vector<placement::PlacementInstance> values;
        std::unordered_map<std::string,std::shared_ptr<const placement::SchematicAsset>> sharedResources;
        auto records=state.resources.records;
        auto nextResource=state.resources.nextId;
        auto requests=resourceRequests(restoreDocument);
        for(size_t i=0;i<requests.size();++i) {
            auto path=requests[i].at("path").get<std::string>();
            if(sharedResources.contains(path))continue;
            auto source=util::pathFromUtf8(path);
            auto found=std::find_if(records.begin(),records.end(),[&](auto const& r){return r.second.path==source;});
            uint64_t id=found==records.end()?nextResource++:found->first;
            if(!id || !nextResource)throw std::runtime_error("Resource ID exhausted.");
            auto asset=std::make_shared<placement::SchematicAsset>(*preparing->assets.at(i).asset);
            asset->resourceId=id;
            sharedResources.emplace(path,asset);
            records.insert_or_assign(id,placement::ResourceRecord{id,source,asset});
        }
        auto const&                               saved = restoreDocument.at("placements");
        for (size_t i = 0; i < saved.size(); ++i) {
            auto const&                  item = saved[i];
            placement::PlacementInstance value;
            value.asset    = sharedResources.at(item.at("path").get<std::string>());
            value.filePath = util::pathFromUtf8(item.at("path").get<std::string>());
            auto [shared,inserted]=sharedResources.try_emplace(item.at("path").get<std::string>(),value.asset);
            if (!inserted) value.asset=shared->second;
            value.name     = item.at("name").get<std::string>();
            value.origin   = readPosition(item.at("origin"));
            int rotation   = session::decodeInteger(item.at("rotation")),
                mirror     = session::decodeInteger(item.at("mirror"));
            if (rotation < 0 || rotation > 3 || mirror < 0 || mirror > 2)
                throw std::runtime_error("Invalid saved transform.");
            value.rotation      = static_cast<placement::PlacementInstance::Rotation>(rotation);
            value.mirror        = static_cast<placement::PlacementInstance::Mirror>(mirror);
            value.enabled       = item.at("enabled").get<bool>();
            value.renderEnabled = item.at("render").get<bool>();
            value.locked = item.value("locked", false);
            value.axisLocked = item.value("axis_locks", std::array<bool, 3>{});
            value.entitiesEnabled = item.value("entities", true);
            if (item.contains("grid")) {
                auto const& grid=item.at("grid");
                value.grid={grid.at("enabled"),grid.at("spacing").get<std::array<int,3>>(),
                    grid.at("negative").get<std::array<int,3>>(),grid.at("positive").get<std::array<int,3>>()};
                if (!value.grid.valid()) throw std::runtime_error("Invalid saved grid.");
            }
            if (item.contains("regions")) for (auto const& [id,settings]:item.at("regions").items()) {
                int r=settings.at("rotation"),m=settings.at("mirror");
                if (r<0 || r>3 || m<0 || m>2) throw std::runtime_error("Invalid saved region transform.");
                value.regions.emplace(id,placement::RegionSettings{settings.at("enabled"),readPosition(settings.at("offset")),r,m});
            }
            for (auto const& entry : item.at("overrides")) {
                placement::OverrideEntry override;
                if (!entry.at("remove").get<bool>()) {
                    auto tag = CompoundTag::fromSnbt(entry.at("block").get<std::string>());
                    if (!tag) throw std::runtime_error("Invalid saved override NBT.");
                    auto block = Block::tryGetFromRegistry(*tag);
                    if (!block) throw std::runtime_error("Saved override block is unavailable.");
                    override.kind  = placement::OverrideEntry::Kind::SetBlock;
                    override.block = &*block;
                }
                auto position = readPosition(entry.at("position"));
                if (!item.value("local_overrides", false)) {
                    auto local = placement::toLocal(value, position);
                    if (!local) throw std::runtime_error("Legacy override coordinate overflow.");
                    position = *local;
                    if (override.block) override.block=placement::localBlock(value,override.block);
                }
                value.overrides.emplace(util::encodePosKey(position), override);
            }
            values.push_back(std::move(value));
        }
        std::optional<size_t> selected;
        if (!restoreDocument.at("selected").is_null()) {
            int index = session::decodeInteger(restoreDocument.at("selected"));
            if (index < 0) throw std::runtime_error("Invalid saved placement selection.");
            selected = static_cast<size_t>(index);
            if (*selected >= values.size()) throw std::runtime_error("Invalid saved placement selection.");
        }
        
        editor::EditorState validated;
        auto                oldSelection = state.selection;
        auto                oldTool      = state.tool;
        auto                oldView      = state.view;
        try {
            restoreView(restoreDocument);
            validated.selection = state.selection;
            validated.tool      = state.tool;
            validated.view      = state.view;
        } catch (...) {
            state.selection = oldSelection;
            state.tool      = oldTool;
            state.view      = oldView;
            throw;
        }
        state.selection = oldSelection;
        state.tool      = oldTool;
        state.view      = oldView;
        store.replaceDimension(dimension, std::move(values), selected);
        state.resources.records.swap(records);state.resources.nextId=nextResource;
        for(auto const& [id,old]:records) {
            auto current=state.resources.records.find(id);
            if(current!=state.resources.records.end() && current->second.content!=old.content)
                store.replaceAsset(old.content,current->second.content);
        }
        store.markResourceChange();
        state.selection = validated.selection;
        state.tool      = validated.tool;
        state.view      = validated.view;
        pending.erase(dimension);
        status.pending   = false;
        status.restoring = false;
        preparing.reset();
        status.detail = "已恢复当前维度 / Current dimension restored";
        capture();
    }
};
SessionPersistence::SessionPersistence(
    editor::EditorState&       state,
    placement::PlacementStore& store,
    std::filesystem::path      directory
)
: mImpl(std::make_unique<Impl>(state, store, std::move(directory))) {}
SessionPersistence::~SessionPersistence() {
    cancelRestore();
    flush();
}
SessionPersistenceStatus SessionPersistence::status() const { return mImpl->status; }
void                     SessionPersistence::cancelRestore() {
    auto& s = *mImpl;
    s.stop.request_stop();
    s.preparing.reset();
    if (s.status.restoring) s.status.detail = "恢复已取消 / Restore cancelled";
    s.status.restoring = false;
}
bool SessionPersistence::requestRestore() {
    auto& s = *mImpl;
    if (!s.status.pending || s.status.restoring
        || (s.reader.valid() && s.reader.wait_for(std::chrono::seconds(0)) != std::future_status::ready))
        return false;
    s.restoreDocument  = s.document["dimensions"].at(std::to_string(s.dimension));
    s.stop             = std::stop_source{};
    auto token         = s.stop.get_token();
    s.reader           = std::async(std::launch::async, [saved = s.restoreDocument, token] {
        Impl::Restored result;
        try {
            for (auto const& value : resourceRequests(saved)) {
                auto path  = util::pathFromUtf8(value.at("path").get<std::string>());
                auto asset = value.contains("draft_ref")
                    ? placement::decodeDraft(saved.at("drafts").at(value.at("draft_ref").get<std::string>()),path)
                    : value.contains("draft") ? placement::decodeDraft(value.at("draft"),path)
                    : placement::SchematicLoader::read(path, token);
                if (!asset) {
                    result.error = asset.error().describe(utf8Path(path));
                    break;
                }
                result.assets.push_back(std::move(asset).value());
            }
        } catch (std::exception const& e) {
            result.error = e.what();
        }
        return result;
    });
    s.status.restoring = true;
    s.status.detail    = "暂存恢复资源 / Preparing restore";
    s.prepareCursor    = 0;
    s.restoreRevisions =
        {s.state.placements.revision, s.state.selection.revision, s.state.view.revision, s.state.tool.revision};
    return true;
}
void SessionPersistence::keepCurrent() {
    auto& s = *mImpl;
    cancelRestore();
    s.pending.erase(s.dimension);
    s.status.pending = false;
    s.status.detail  = "使用当前维度状态 / Keeping current state";
    s.capture();
}
void SessionPersistence::tick(ClientInstance& client, util::WorkBudget const& budget) {
    auto& s      = *mImpl;
    auto* player = client.getLocalPlayer();
    if (!player) return;
    try {
        if (!s.initialized) {
            auto identity = session::worldIdentity(client);
            if (identity.empty()) {
                s.status.detail = "稳定世界/服务器身份不可用 / Stable identity unavailable";
                return;
            }
            s.status.identity = identity;
            s.initialized     = true;
            s.file            = s.directory / "sessions" / (session::identityFileKey(identity) + ".json");
            s.document        = {
                {"version",    session::DocumentVersion},
                {"identity",   identity                },
                {"dimensions", Json::object()          }
            };
            if (std::filesystem::exists(s.file)) {
                std::ifstream input(s.file);
                auto          doc = Json::parse(input);
                session::validateDocument(doc, identity);
                s.document = std::move(doc);
                s.document["version"]=session::DocumentVersion;
                for (auto const& [key, value] : s.document["dimensions"].items()) s.pending.insert(std::stoi(key));
            }
            s.status.available = true;
        }
        if (!s.status.available) return;
        int dimension = static_cast<int>(player->getDimensionId());
        if (dimension != s.dimension) {
            s.capture();
            cancelRestore();
            s.dimension = dimension;
            if (auto it = s.live.find(dimension); it != s.live.end()) {
                s.restoreView(it->second);
                std::optional<size_t> selected;
                if (!it->second["selected"].is_null()) selected = it->second["selected"].get<size_t>();
                size_t index = 0;
                s.store.deselect();
                for (auto id : s.state.placements.order) {
                    if (s.state.placements.placements.at(id).dimensionId != dimension) continue;
                    if (selected == index) (void)s.store.select(id);
                    ++index;
                }
            } else {
                selection::SelectionState selection;
                selection.revision      = s.state.selection.revision + 1;
                s.state.selection       = selection;
                s.state.view.layerRange = {};
                ++s.state.view.revision;
                s.store.deselect();
            }
            s.status.pending = s.pending.contains(dimension);
            s.status.placements =
                s.status.pending ? s.document["dimensions"].at(std::to_string(dimension)).at("placements").size() : 0;
            s.status.detail = s.status.pending ? "发现会话记录，请在会话菜单手动恢复 / Saved session available"
                                               : "会话存档就绪 / Session storage ready";
            if (s.status.pending) player->displayClientMessage(s.status.detail, std::nullopt);
            s.revisions[0] = UINT64_MAX;
        }
        if (s.status.restoring
            && s.restoreRevisions
                   != std::array<uint64_t, 4>{
                       s.state.placements.revision,
                       s.state.selection.revision,
                       s.state.view.revision,
                       s.state.tool.revision
                   }) {
            cancelRestore();
            s.status.detail = "恢复期间状态已变化，请重新确认 / State changed during restore; retry";
        }
        if (s.status.restoring && s.reader.valid()
            && s.reader.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            s.preparing = s.reader.get();
            if (!s.preparing->error.empty()) throw std::runtime_error(s.preparing->error);
        }
        if (s.status.restoring && s.preparing) {
            while (s.prepareCursor < s.preparing->assets.size() && budget.available()) {
                if (!placement::SchematicLoader::advance(s.preparing->assets[s.prepareCursor], budget)) break;
                ++s.prepareCursor;
            }
            if (s.prepareCursor == s.preparing->assets.size() && budget.available()) s.publish();
        }
        std::array<uint64_t, 4> revisions{
            s.state.placements.revision,
            s.state.selection.revision,
            s.state.view.revision,
            s.state.tool.revision
        };
        if (revisions != s.revisions) {
            s.capture();
            s.revisions = revisions;
        }
        if (s.writer.valid() && s.writer.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto error = s.writer.get();
            if (!error.empty()) {
                s.status.detail = error;
                s.dirty         = true;
                s.changed       = std::chrono::steady_clock::now();
            }
        }
        if (s.dirty && !s.writer.valid() && std::chrono::steady_clock::now() - s.changed >= std::chrono::seconds(2)) {
            auto bytes = s.document.dump(2);
            auto path  = s.file;
            s.dirty    = false;
            s.writer   = std::async(std::launch::async, [path, bytes = std::move(bytes)] {
                try {
                    util::writeAtomic(path, bytes);
                    return std::string{};
                } catch (std::exception const& e) {
                    return std::string(e.what());
                }
            });
        }
    } catch (std::exception const& e) {
        cancelRestore();
        s.status.detail = e.what();
    }
}
void SessionPersistence::flush() noexcept {
    auto& s = *mImpl;
    try {
        if (s.writer.valid()) {
            auto error = s.writer.get();
            if (!error.empty()) s.dirty = true;
        }
        if (s.revisions
            != std::array<uint64_t, 4>{
                s.state.placements.revision,
                s.state.selection.revision,
                s.state.view.revision,
                s.state.tool.revision
            })
            s.capture();
        if (s.dirty && s.status.available) {
            util::writeAtomic(s.file, s.document.dump(2));
            s.dirty = false;
        }
    } catch (std::exception const& e) {
        s.status.detail = e.what();
    }
}
} 
