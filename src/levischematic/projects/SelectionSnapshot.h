#pragma once
#include "ProjectDocument.h"
#include "levischematic/selection/SelectionState.h"
#include <set>
namespace levischematic::projects {
inline Json     encodePosition(BlockPos p) { return Json::array({p.x, p.y, p.z}); }
inline BlockPos decodePosition(Json const& value) {
    auto a = session::decodePosition(value);
    return {a[0], a[1], a[2]};
}
inline Json encodeSelection(selection::SelectionState const& s) {
    auto optional = [](std::optional<BlockPos> p) -> Json { return p ? encodePosition(*p) : Json(nullptr); };
    Json value{
        {"normal",   s.normal                },
        {"corner1",  optional(s.corner1)     },
        {"corner2",  optional(s.corner2)     },
        {"origin",   optional(s.customOrigin)},
        {"selected", s.selectedRegion        },
        {"next",     s.nextRegion            },
        {"regions",  Json::array()           }
    };
    for (auto const& r : s.regions)
        value["regions"].push_back({
            {"id",      r.id               },
            {"name",    r.name             },
            {"corner1", optional(r.corner1)},
            {"corner2", optional(r.corner2)}
        });
    return value;
}
inline selection::SelectionState decodeSelection(Json const& v) {
    selection::SelectionState s;
    auto                      optional = [](Json const& j) -> std::optional<BlockPos> {
        return j.is_null() ? std::nullopt : std::optional{decodePosition(j)};
    };
    s.normal         = v.at("normal").get<bool>();
    s.corner1        = optional(v.at("corner1"));
    s.corner2        = optional(v.at("corner2"));
    s.customOrigin   = optional(v.at("origin"));
    s.selectedRegion = v.at("selected").get<uint32_t>();
    s.nextRegion     = v.at("next").get<uint32_t>();
    if (v.at("regions").size() > 4096) throw std::runtime_error("Too many saved selection regions.");
    std::set<uint32_t> ids;
    for (auto const& r : v.at("regions")) {
        auto id = r.at("id").get<uint32_t>();
        if (!id || !ids.insert(id).second || id >= s.nextRegion)
            throw std::runtime_error("Invalid saved selection region ID.");
        s.regions.push_back(
            {id, r.at("name").get<std::string>(), optional(r.at("corner1")), optional(r.at("corner2"))}
        );
    }
    if (s.selectedRegion && !ids.contains(s.selectedRegion)) throw std::runtime_error("Saved selected region missing.");
    return s;
}
} 
