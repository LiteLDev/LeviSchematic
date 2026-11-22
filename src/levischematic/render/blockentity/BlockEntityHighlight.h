#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace levischematic::render {


std::uintptr_t resolveProjectionEntry(char const* symbol, char const* signature);


void registerBlockEntityHighlight();
void unregisterBlockEntityHighlight();

struct BlockEntityHighlightStats {
    std::size_t submitted         = 0;
    std::size_t highlighted       = 0;
    std::size_t forwardVariations = 0;
    std::string skipped;
};


BlockEntityHighlightStats submitBlockEntityHighlight(std::function<std::size_t()> const& dispatch);


bool                      isProjectionPrepass();
bool                      isProjectionDeferred();
BlockEntityHighlightStats submitBlockEntityPrepass(std::function<std::size_t()> const& dispatch);

struct ValidationMarkerRouteStats {
    std::size_t routed   = 0;
    std::size_t unrouted = 0;
};

ValidationMarkerRouteStats routeValidationMarkers(std::function<void()> const& dispatch, std::uint16_t view);

ValidationMarkerRouteStats labelInsertedRecords(std::function<void()> const& dispatch, std::uint16_t view);

bool validationMarkerHighlightAvailable();

bool projectionBlendDepthAvailable();

} // namespace levischematic::render
