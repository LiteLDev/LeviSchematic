#include "levischematic/app/ConfigCompatibility.h"
#include "levischematic/app/ConfigStore.h"
#include "levischematic/app/LoadTaskGate.h"
#include "levischematic/render/HullCacheState.h"
#include "levischematic/util/WorkBudget.h"
#include "nlohmann/json.hpp"
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace levischematic;


void testConfiguration() {
    app::ConfigStore store;
    auto             config = store.snapshot();
    assert(config.performance.geometryBuildsPerFrame == 1);
    assert(config.performance.cameraResortsPerFrame == 8);
    assert(config.performance.queuedChunkBuilds == 4);
    assert(config.visibility.renderDistanceBlocks == 128 && config.visibility.frustumCulling);
    config.projection.alpha         = 0.6f;
    config.performance.tickBudgetMs = 3;
    std::thread external([&] { assert(store.submit(config).empty()); });
    external.join();
    assert(store.snapshot().projection.alpha == 0.15f);
    auto invalid               = config;
    invalid.hull.blockColor[1] = std::numeric_limits<float>::quiet_NaN();
    assert(!store.submit(invalid).empty());
    auto applied = store.applyPending();
    assert(applied && applied->projection.alpha == 0.6f);
    applied->projection.alpha = 0;
    assert(store.snapshot().projection.alpha == 0.6f);
    assert(!store.applyPending());
    invalid                             = config;
    invalid.performance.maxLoadRequests = 0;
    assert(!store.submit(invalid).empty());
    invalid                           = config;
    invalid.performance.frameBudgetMs = -1;
    assert(!store.submit(invalid).empty());
    invalid         = config;
    invalid.version = 2;
    assert(!store.submit(invalid).empty());
    invalid = config;
    invalid.projection.texture.clear();
    assert(!store.submit(invalid).empty());
    invalid                = config;
    invalid.hull.expansion = 1;
    assert(!store.submit(invalid).empty());
    assert(!store.applyPending());
    config.hull.propertyColor[3] = 0.2f;
    assert(store.submit(config).empty());
    assert(store.applyPending()->hull.propertyColor[3] == 0.2f);
    invalid                                 = config;
    invalid.visibility.renderDistanceBlocks = -1;
    assert(!store.submit(invalid).empty());
    assert(!store.applyPending());
    config.visibility.renderDistanceBlocks = 0;
    config.visibility.frustumCulling       = false;
    assert(store.submit(config).empty());
    assert(store.snapshot().visibility.renderDistanceBlocks == 128);
    auto visibility = store.applyPending()->visibility;
    assert(visibility.renderDistanceBlocks == 0 && !visibility.frustumCulling);
}

void testSelectionConfiguration() {
    app::ConfigStore store;
    auto             config        = store.snapshot();
    auto const       originalColor = config.selection.faceColor;
    assert((originalColor == std::array<float, 4>{1.0f, 1.0f, 1.0f, 0.10f}));
    config.selection.faceColor = {0.2f, 0.4f, 0.6f, 0.25f};
    assert(store.submit(config).empty());
    assert(store.snapshot().selection.faceColor == originalColor);
    for (std::size_t channel = 0; channel < originalColor.size(); ++channel) {
        for (float invalidValue :
             {-0.1f, 1.1f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
            auto invalid                         = config;
            invalid.selection.faceColor[channel] = invalidValue;
            assert(!store.submit(invalid).empty());
        }
    }
    auto applied = store.applyPending();
    assert(applied && applied->selection == config.selection);
    assert(!store.applyPending());
    config.selection.faceColor = {0.0f, 1.0f, 0.0f, 0.0f};
    assert(store.submit(config).empty());
    assert(store.applyPending()->selection == config.selection);
    config.selection.faceColor[3] = 1.0f;
    assert(store.submit(config).empty());
    assert(store.applyPending()->selection == config.selection);
}

void testSelectionCompatibility() {
    auto legacy       = nlohmann::ordered_json::object();
    legacy["version"] = 1;
    app::applySelectionDefaults(legacy);
    assert(legacy["selection"]["faceColor"] == nlohmann::ordered_json(app::Config{}.selection.faceColor));
    legacy["selection"] = nlohmann::ordered_json::object();
    app::applySelectionDefaults(legacy);
    assert(legacy["selection"]["faceColor"] == nlohmann::ordered_json(app::Config{}.selection.faceColor));
    legacy["selection"]["faceColor"] = {0.2f, 0.4f, 0.6f, 0.25f};
    auto const custom                = legacy;
    app::applySelectionDefaults(legacy);
    assert(legacy == custom);
    legacy["selection"]["faceColor"] = "invalid";
    app::applySelectionDefaults(legacy);
    assert(legacy["selection"]["faceColor"].is_string());
    legacy["selection"] = nullptr;
    app::applySelectionDefaults(legacy);
    assert(legacy["selection"].is_null());
    legacy = nlohmann::ordered_json::array();
    app::applySelectionDefaults(legacy);
    assert(legacy.is_array());
}

void testVisibilityCompatibility() {
    auto legacy       = nlohmann::ordered_json::object();
    legacy["version"] = 1;
    app::applyVisibilityDefaults(legacy);
    assert(legacy["visibility"]["renderDistanceBlocks"] == 128);
    assert(legacy["visibility"]["frustumCulling"] == true);
    legacy["visibility"] = {
        {"renderDistanceBlocks", 48}
    };
    app::applyVisibilityDefaults(legacy);
    assert(legacy["visibility"]["renderDistanceBlocks"] == 48);
    assert(legacy["visibility"]["frustumCulling"] == true);
    legacy["visibility"]["frustumCulling"] = false;
    app::applyVisibilityDefaults(legacy);
    assert(legacy["visibility"]["frustumCulling"] == false);
    legacy["visibility"] = "invalid";
    app::applyVisibilityDefaults(legacy);
    assert(legacy["visibility"].is_string());
}

void testHullCacheVersions() {
    render::detail::HullCacheState cache{7, 9, app::Config{}.hull};
    auto                           style = app::Config{}.hull;
    assert(cache.matches(7, 9, style));
    assert(!cache.matches(8, 9, style));
    assert(!cache.matches(7, 10, style));
    style.propertyColor[3] = 0;
    assert(!cache.matches(7, 9, style));
    style           = app::Config{}.hull;
    style.expansion = 0.01f;
    assert(!cache.matches(7, 9, style));
    render::detail::RenderBatchVersion batch;
    assert(!batch.matches(0));
    batch.revision = 42;
    assert(!batch.matches(42));
    batch.complete = true;
    assert(batch.matches(42));
    assert(!batch.matches(43));
    batch = {};
    assert(!batch.matches(42));
}


void testTaskPublication() {
    app::LoadTaskGate cancelled(7);
    assert(cancelled.cancel());
    assert(cancelled.token().stop_requested());
    assert(!cancelled.publish(7, [] { assert(false); }));
    app::LoadTaskGate stale(7);
    assert(!stale.publish(8, [] { assert(false); }));
    assert(stale.cancel());
    app::LoadTaskGate failure(9);
    bool              caught = false;
    try {
        failure.publish(9, [] { throw std::runtime_error("allocation failed"); });
    } catch (std::runtime_error const&) {
        caught = true;
    }
    assert(caught && failure.cancel());
    for (int iteration = 0; iteration != 200; ++iteration) {
        app::LoadTaskGate racing(11);
        std::barrier      start(2);
        bool              wasCancelled = false;
        int               commits      = 0;
        std::thread       cancel([&] {
            start.arrive_and_wait();
            wasCancelled = racing.cancel();
        });
        start.arrive_and_wait();
        bool published = racing.publish(11, [&] { ++commits; });
        cancel.join();
        assert(published != wasCancelled);
        assert(commits == (published ? 1 : 0));
        assert(!racing.publish(11, [] { assert(false); }));
    }
}


void testBudget() {
    util::WorkBudget expired(0);
    assert(!expired.available());
    util::WorkBudget shared(2);
    std::this_thread::sleep_for(std::chrono::milliseconds(4));
    assert(!shared.available());
    auto const& nextStage = shared;
    assert(!nextStage.available() && nextStage.elapsedMs() >= 2);
    util::WorkBudget nextTick(1000);
    assert(nextTick.available());
}


int main() {
    testConfiguration();
    testSelectionConfiguration();
    testSelectionCompatibility();
    testVisibilityCompatibility();
    testHullCacheVersions();
    testTaskPublication();
    testBudget();
    std::cout << "Refactor state tests passed\n";
}
