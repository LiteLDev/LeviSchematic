#pragma once

#include "levischematic/app/ConfigStore.h"
#include "levischematic/app/PlacementService.h"
#include "levischematic/app/RuntimeContext.h"
#include "levischematic/app/SchematicLoadService.h"
#include "levischematic/app/SelectionService.h"
#include "levischematic/app/SessionGate.h"
#include "levischematic/app/ViewService.h"
#include "levischematic/input/InputHandler.h"
#include "levischematic/verifier/VerifierService.h"

#include <atomic>
#include <memory>
#include <optional>

class ClientInstance;
namespace levischematic::render {
class SchematicChunkRenderer;
}

namespace levischematic::app {


class AppKernel {
public:
    enum class State { Empty, Starting, Active, Stopping };


    AppKernel();


    ~AppKernel();


    AppKernel(AppKernel const&) = delete;


    AppKernel& operator=(AppKernel const&) = delete;


    void invalidateRuntime(std::optional<int> targetDimension = {});


    void resetRenderer();


    void invalidateRenderResources();


    [[nodiscard]] bool hasSession() const noexcept;


    [[nodiscard]] State state() const noexcept { return mState; }


    [[nodiscard]] uint64_t resourceGeneration() const noexcept;


    [[nodiscard]] uint64_t sessionGeneration() const noexcept { return mGate.generation(); }


    [[nodiscard]] bool runtimeReady() const noexcept;


    [[nodiscard]] bool usesClientCommands() const noexcept;


    [[nodiscard]] RuntimeContext& runtime();


    [[nodiscard]] PlacementService& placement();


    [[nodiscard]] SelectionService& selection();


    [[nodiscard]] ViewService& view();


    [[nodiscard]] render::SchematicChunkRenderer& schematicRenderer();


    [[nodiscard]] verifier::VerifierService& verifier();


    [[nodiscard]] input::InputHandler& input();

    [[nodiscard]] SchematicLoadService& loads() { return mLoads; }

private:
    friend class SessionAccess;


    friend void start();


    friend void clientReady(ClientInstance&);


    friend void clientTick(ClientInstance&);


    friend void drainSession() noexcept;


    void startSession(ClientInstance& client);


    void requestSession(ClientInstance& client);


    void endSession();


    void maintain(ClientInstance& client);
    class WorldSession;
    std::unique_ptr<WorldSession> mSession;
    SchematicLoadService          mLoads;
    State                         mState              = State::Empty;
    bool                          mUsesClientCommands = false;
    bool                          mReadyPending       = false;
    std::atomic_uint64_t          mResourceGeneration{0};
    SessionGate&                  mGate;
};


class SessionAccess {
public:
    explicit SessionAccess(bool tryOnly = false);


    SessionAccess(SessionAccess&&) noexcept = default;


    SessionAccess& operator=(SessionAccess&&) noexcept = default;


    SessionAccess(SessionAccess const&) = delete;


    SessionAccess& operator=(SessionAccess const&) = delete;


    void reset() noexcept {
        mKernel = nullptr;
        mLock   = {};
    }


    [[nodiscard]] explicit operator bool() const noexcept { return mKernel != nullptr && mLock.owns_lock(); }


    [[nodiscard]] AppKernel* operator->() const noexcept { return mKernel; }


    [[nodiscard]] AppKernel& operator*() const noexcept { return *mKernel; }

private:
    SessionGate::Lock mLock;
    AppKernel*        mKernel = nullptr;
};


[[nodiscard]] SessionAccess accessSession();


[[nodiscard]] SessionAccess tryAccessSession();


[[nodiscard]] bool hasAppKernel();


[[nodiscard]] bool hasActiveSession();


void load();


void start();


void stop();


void clientReady(ClientInstance& client);


void clientLeaving(ClientInstance& client);


void clientTick(ClientInstance& client);


void invalidateRuntime(std::optional<int> targetDimension = {});


void beginResourceReload();


void resourceReloadQueued();


void sourceDestroyed(verifier_block_listener::VerifierBlockListener& listener, BlockSource& source);

} 
