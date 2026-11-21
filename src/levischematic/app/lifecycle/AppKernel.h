#pragma once

#include "levischematic/app/lifecycle/SessionGate.h"
#include "levischematic/interaction/input/InputHandler.h"
#include "levischematic/interaction/sign/SignService.h"
#include "levischematic/interaction/survival/service/SurvivalService.h"
#include "levischematic/interaction/tool/ToolService.h"
#include "levischematic/interaction/worldedit/service/WorldEditService.h"
#include "levischematic/schematic/convert/ConversionService.h"
#include "levischematic/schematic/io/RuntimeContext.h"
#include "levischematic/schematic/placement/service/PlacementService.h"
#include "levischematic/schematic/placement/view/ViewService.h"
#include "levischematic/schematic/resource/service/SchematicLoadService.h"
#include "levischematic/selection/service/SelectionSaveService.h"
#include "levischematic/selection/service/SelectionService.h"
#include "levischematic/verifier/service/VerifierService.h"
#include "levischematic/workspace/config/ConfigStore.h"
#include "levischematic/workspace/project/service/ProjectService.h"
#include "levischematic/workspace/session/service/SessionPersistence.h"

#include <atomic>
#include <memory>
#include <optional>

class ClientInstance;
namespace levischematic::render {
class SchematicChunkRenderer;
class ActorProjection;
}

namespace levischematic::app {

class AppKernel {
public:
    enum class State { Empty, Starting, Active, Stopping };
    AppKernel();
    ~AppKernel();
    AppKernel(AppKernel const&)            = delete;
    AppKernel& operator=(AppKernel const&) = delete;

    void                   invalidateRuntime(std::optional<int> targetDimension = {});
    void                   resetRenderer();
    void                   invalidateRenderResources();
    [[nodiscard]] bool     hasSession() const noexcept;
    [[nodiscard]] State    state() const noexcept { return mState; }
    [[nodiscard]] uint64_t resourceGeneration() const noexcept;
    [[nodiscard]] uint64_t sessionGeneration() const noexcept { return mGate.generation(); }
    [[nodiscard]] bool     runtimeReady() const noexcept;
    [[nodiscard]] bool     hudReady() const noexcept;
    [[nodiscard]] bool     usesClientCommands() const noexcept;

    [[nodiscard]] RuntimeContext&                 runtime();
    [[nodiscard]] PlacementService&               placement();
    [[nodiscard]] SelectionService&               selection();
    [[nodiscard]] ViewService&                    view();
    [[nodiscard]] render::SchematicChunkRenderer& schematicRenderer();
    render::ActorProjection&                      actors();
    [[nodiscard]] verifier::VerifierService&      verifier();
    [[nodiscard]] input::InputHandler&            input();
    [[nodiscard]] ToolService&                    tools();
    [[nodiscard]] SurvivalService&                survival();
    [[nodiscard]] WorldEditService&               worldEdit();
    [[nodiscard]] ProjectService&                 projects();
    [[nodiscard]] ProjectServices                 projectServices();
    [[nodiscard]] SignService&                    signs();
    [[nodiscard]] SessionPersistence&             sessions();
    [[nodiscard]] SelectionSaveService&           saves() { return mSaves; }
    [[nodiscard]] SchematicLoadService&           loads() { return mLoads; }
    [[nodiscard]] convert::ConversionService&     conversions() { return mConversions; }

private:
    friend class SessionAccess;
    friend void start();
    friend void clientReady(ClientInstance&);
    friend void clientTick(ClientInstance&);
    friend void drainSession() noexcept;
    void        startSession(ClientInstance& client);
    void        requestSession(ClientInstance& client);
    void        endSession();
    void        maintain(ClientInstance& client);
    class WorldSession;
    std::unique_ptr<WorldSession> mSession;
    SchematicLoadService          mLoads;
    convert::ConversionService    mConversions;
    SelectionSaveService          mSaves;
    uint64_t                      mSaveNotified       = 0;
    State                         mState              = State::Empty;
    bool                          mUsesClientCommands = false;
    bool                          mReadyPending       = false;
    std::atomic_uint64_t          mResourceGeneration{0};
    SessionGate&                  mGate;
};

class SessionAccess {
public:
    explicit SessionAccess(bool tryOnly = false);
    SessionAccess(SessionAccess&&) noexcept            = default;
    SessionAccess& operator=(SessionAccess&&) noexcept = default;
    SessionAccess(SessionAccess const&)                = delete;
    SessionAccess& operator=(SessionAccess const&)     = delete;
    void           reset() noexcept {
        mKernel = nullptr;
        mLock   = {};
    }
    [[nodiscard]] explicit   operator bool() const noexcept { return mKernel != nullptr && mLock.owns_lock(); }
    [[nodiscard]] AppKernel* operator->() const noexcept { return mKernel; }
    [[nodiscard]] AppKernel& operator*() const noexcept { return *mKernel; }

private:
    SessionGate::Lock mLock;
    AppKernel*        mKernel = nullptr;
};

[[nodiscard]] SessionAccess accessSession();
[[nodiscard]] SessionAccess tryAccessSession();
[[nodiscard]] bool          hasAppKernel();
[[nodiscard]] bool          hasActiveSession();
void                        load();
void                        start();
void                        stop();
void                        clientReady(ClientInstance& client);
void                        clientLeaving(ClientInstance& client);
void                        clientTick(ClientInstance& client);
void                        invalidateRuntime(std::optional<int> targetDimension = {});

void beginResourceReload();
void resourceReloadQueued();
void sourceDestroyed(verifier_block_listener::VerifierBlockListener& listener, BlockSource& source);

}
