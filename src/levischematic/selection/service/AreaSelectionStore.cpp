#include "levischematic/selection/service/AreaSelectionStore.h"
#include "levischematic/shared/i18n/I18n.h"
#include <fstream>
#include <unordered_set>

namespace levischematic::selection {
namespace {
using Json = nlohmann::json;
Json position(std::optional<BlockPos> const& pos) {
    if (!pos) return nullptr;
    return Json::array({pos->x, pos->y, pos->z});
}
std::optional<BlockPos> readPosition(Json const& value) {
    if (value.is_null()) return {};
    if (!value.is_array() || value.size() != 3) throw std::runtime_error("Invalid position.");
    return BlockPos{value[0].get<int>(), value[1].get<int>(), value[2].get<int>()};
}
} // namespace

Json encodeAreaSelection(SelectionState const& state) {
    Json result{
        {"format",          AreaSelectionFormat         },
        {"name",            state.name                  },
        {"normal",          state.normal                },
        {"corner1",         position(state.corner1)     },
        {"corner2",         position(state.corner2)     },
        {"origin",          position(state.customOrigin)},
        {"selected_region", state.selectedRegion        },
        {"regions",         Json::array()               }
    };
    for (auto const& r : state.regions)
        result["regions"].push_back({
            {"id",      r.id               },
            {"name",    r.name             },
            {"corner1", position(r.corner1)},
            {"corner2", position(r.corner2)}
        });
    return result;
}

SelectionState decodeAreaSelection(Json const& value, SelectionState base) {
    if (!value.is_object() || value.value("format", 0) != AreaSelectionFormat)
        throw std::runtime_error("Unsupported area selection file.");
    base.name         = value.value("name", std::string("Unnamed"));
    base.normal       = value.value("normal", false);
    base.corner1      = readPosition(value.value("corner1", Json()));
    base.corner2      = readPosition(value.value("corner2", Json()));
    base.customOrigin = readPosition(value.value("origin", Json()));
    base.regions.clear();
    std::unordered_set<uint32_t> ids;
    uint32_t                     next = 1;
    for (auto const& r : value.value("regions", Json::array())) {
        uint32_t id = r.at("id").get<uint32_t>();
        if (!id || id == UINT32_MAX || !ids.insert(id).second) throw std::runtime_error("Invalid region ID.");
        base.regions.push_back(
            {id, r.at("name").get<std::string>(), readPosition(r.at("corner1")), readPosition(r.at("corner2"))}
        );
        next = std::max(next, id + 1);
    }
    base.nextRegion     = next;
    base.selectedRegion = value.value("selected_region", uint32_t{0});
    if (base.selectedRegion && !ids.contains(base.selectedRegion))
        base.selectedRegion = base.regions.empty() ? 0 : base.regions.front().id;
    if (base.normal && base.regions.empty()) base.normal = false;
    if (base.normal) {
        auto it = std::find_if(base.regions.begin(), base.regions.end(), [&](auto const& r) {
            return r.id == base.selectedRegion;
        });
        if (it == base.regions.end()) it = base.regions.begin();
        base.selectedRegion = it->id;
        base.corner1        = it->corner1;
        base.corner2        = it->corner2;
    }
    ++base.revision;
    return base;
}

std::string saveAreaSelection(std::filesystem::path const& file, SelectionState const& state) {
    try {
        std::filesystem::create_directories(file.parent_path());
        auto temporary  = file;
        temporary      += ".tmp";
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            out << encodeAreaSelection(state).dump(2) << '\n';
            if (!out.flush()) return tr("msg.service.cannot_write_selection_file");
        }
        std::error_code ec;
        std::filesystem::rename(temporary, file, ec);
        if (ec) return tr("msg.service.cannot_save_selection_file") + ec.message();
        return {};
    } catch (std::exception const& e) {
        return e.what();
    }
}

std::optional<SelectionState>
loadAreaSelection(std::filesystem::path const& file, SelectionState const& base, std::string& error) {
    try {
        std::ifstream input(file, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open selection file.");
        return decodeAreaSelection(Json::parse(input), base);
    } catch (std::exception const& e) {
        error = e.what();
        return {};
    }
}

std::string describeAreaSelection(std::filesystem::path const& file) {
    std::string error;
    auto        state = loadAreaSelection(file, {}, error);
    if (!state) return error;
    std::string origin = tr("ui.browser.area_origin_auto");
    if (state->customOrigin)
        origin = std::to_string(state->customOrigin->x) + ", " + std::to_string(state->customOrigin->y) + ", "
               + std::to_string(state->customOrigin->z);
    size_t boxes = state->normal ? state->regions.size() : (state->corner1 && state->corner2 ? 1 : 0);
    return tr("ui.browser.area_description", state->name, origin, boxes);
}
} // namespace levischematic::selection
