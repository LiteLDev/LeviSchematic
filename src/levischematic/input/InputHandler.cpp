#include "InputHandler.h"
#include "Actions.h"
#include "BindingRules.h"
#include "ChordState.h"
#include "levischematic/app/AppKernel.h"
#include "levischematic/app/ToolService.h"
#include "levischematic/ui/Bindings.h"
#include "levischematic/ui/Ui.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/input/KeyInputEvent.h"
#include "ll/api/event/input/MouseInputEvent.h"
#include "ll/api/input/KeyRegistry.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/input/HIDController.h"
#include "mc/world/Facing.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/VanillaItemNames.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ShapeType.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/phys/HitResult.h"
#include <Windows.h>
#include <deque>
#include <mutex>

namespace levischematic::input {
namespace {
struct PendingAction {
    ActionId id;
    uint64_t generation;
};
struct Runtime {
    std::mutex                mutex;
    ChordState                chords;
    std::deque<PendingAction> pending;
    ll::event::ListenerPtr    keyboard, mouse;
    bool                      registered = false;
};
Runtime& runtime() {
    static Runtime state;
    return state;
}
bool gameplay(IClientInstance& client) {
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId() && client.getLocalPlayer() && client.getMouseGrabbed()
        && client.isInWorldAndNotShowingAnyMenuScreens() && !ui::isOpen();
}
bool hasTool(IClientInstance& client, app::AppKernel& session) {
    auto player = client.getLocalPlayer();
    return session.tools().state().enabled && player
        && player->getSelectedItem().isInstance(VanillaItemNames::Stick(), false);
}
void enqueue(ActionId id) {
    auto session = app::accessSession();
    auto client  = ll::service::getClientInstance();
    if (!session || !client || !gameplay(*client)) return;
    auto&           r = runtime();
    std::lock_guard lock(r.mutex);
    if (r.pending.size() < 128) r.pending.push_back({id, session->sessionGeneration()});
}
bool feed(int key, bool down, bool textFocused = false) {
    auto session  = app::accessSession();
    auto client   = ll::service::getClientInstance();
    bool active   = session && client && gameplay(*client) && !textFocused;
    auto bindings = ui::inputBindings();
    if (active) {
        auto cfg           = app::configStore().snapshot().survival;
        bool tool          = hasTool(*client, *session);
        bool target        = false;
        bool queriesTarget = std::any_of(bindings.begin(), bindings.end(), [&](auto const& b) {
            return (b.id == ActionId::PickBlock || b.id == ActionId::EasyPlace)
                && std::find(b.keys.begin(), b.keys.end(), canonicalKey(key)) != b.keys.end();
        });
        if ((cfg.pickBlock || cfg.easyPlace) && !tool && queriesTarget) target = session->survival().hasTarget(*client);
        std::erase_if(bindings, [&](auto const& b) {
            if (session->worldEdit().ownsInteraction()
                && (b.id == ActionId::PickBlock || b.id == ActionId::EasyPlace || b.id == ActionId::EasyPlaceRelease))
                return true;
            if (b.id == ActionId::PickBlock) return !cfg.pickBlock || tool || !target;
            if (b.id == ActionId::EasyPlace || b.id == ActionId::EasyPlaceRelease)
                return !cfg.easyPlace || tool || (!target && down);
            return false;
        });
    }
    auto&           r = runtime();
    std::lock_guard lock(r.mutex);
    auto decision = r.chords.feed(canonicalKey(key), down, active, active && hasTool(*client, *session), bindings);
    if (decision.action && r.pending.size() < 128)
        r.pending.push_back({*decision.action, session->sessionGeneration()});
    return decision.consume;
}
} 

void registerInputDefinitions() {
    auto& r = runtime();
    if (r.registered) return;
    auto& registry = ll::input::KeyRegistry::getInstance();
    for (auto const& spec : actionSpecs()) {
        
        auto  defaults = spec.keys.size() == 1 && spec.keys[0] >= -3 ? spec.keys : std::vector<int>{};
        auto& handle   = registry.getOrCreateKey(spec.name, defaults, true, ll::mod::NativeMod::current());
        if (spec.sdkCallback)
            handle.registerButtonDownHandler([id = spec.id](::FocusImpact, IClientInstance&) { enqueue(id); });
    }
    r.registered = true;
}
void startInputEvents() {
    auto& r = runtime();
    if (r.keyboard) return;
    auto& bus  = ll::event::EventBus::getInstance();
    r.keyboard = bus.emplaceListener<ll::event::KeyInputEvent>([](auto& event) {
        if (event.isCancelled()) return;
        if (feed(event.keyCode(), event.isDown(), event.controller().mTextboxIsFocused)) event.cancel();
    });
    r.mouse    = bus.emplaceListener<ll::event::MouseInputEvent>([](auto& event) {
        if (event.isCancelled()) return;
        int button = event.actionButtonId();
        if (button == 4) {
            if (event.buttonData() == 0) return;
            int  key     = event.buttonData() > 0 ? WheelUp : WheelDown;
            bool consume = feed(key, true);
            (void)feed(key, false);
            if (consume) event.cancel();
        } else if (button >= 1 && button <= 3) {
            if (feed(-button, event.buttonData() == 1)) event.cancel();
        }
    });
}
void resetInputState() {
    auto&           r = runtime();
    std::lock_guard lock(r.mutex);
    
    r.chords.deactivate();
    r.pending.clear();
}
void stopInputEvents() {
    auto& r   = runtime();
    auto& bus = ll::event::EventBus::getInstance();
    if (r.keyboard) bus.removeListener<ll::event::KeyInputEvent>(r.keyboard);
    if (r.mouse) bus.removeListener<ll::event::MouseInputEvent>(r.mouse);
    r.keyboard.reset();
    r.mouse.reset();
    std::lock_guard lock(r.mutex);
    r.chords.reset();
    r.pending.clear();
}
bool InputExecutionContext::isValid() const { return viewService && static_cast<bool>(refreshProjectionState); }
void InputHandler::initialize(InputExecutionContext context) {
    mContext = std::move(context);
    resetInputState();
}
void InputHandler::shutdown() {
    resetInputState();
    mContext = {};
}
bool InputHandler::isInitialized() const { return mContext.isValid(); }
void InputHandler::pump(IClientInstance& client, uint64_t generation) {
    std::deque<PendingAction> actions;
    {
        auto&           r = runtime();
        std::lock_guard lock(r.mutex);
        if (!gameplay(client)) {
            r.chords.deactivate();
            r.pending.clear();
            return;
        }
        actions.swap(r.pending);
    }
    auto* tools  = mContext.toolService;
    auto* player = client.getLocalPlayer();
    if (!isInitialized() || !tools || !player) return;
    for (auto const& action : actions) {
        if (action.generation != generation) continue;
        auto const& spec = actionSpec(action.id);
        if (spec.tool
            && (!tools->state().enabled || !player->getSelectedItem().isInstance(VanillaItemNames::Stick(), false)))
            continue;
        switch (action.id) {
        case ActionId::WorldOperations:
            ui::requestOpen(ui::PageId::WorldOperations);
            return;
        case ActionId::Projects:
            ui::requestOpen(ui::PageId::Projects);
            return;
        case ActionId::ExecuteOperation: {
            auto s = app::accessSession();
            if (!s) break;
            if (editor::creativeTool(tools->state().activeTool))
                s->worldEdit().options().operation = static_cast<worldedit::Operation>(
                    static_cast<int>(tools->state().activeTool) - static_cast<int>(editor::ToolMode::Paste)
                );
            if (s->worldEdit().progress().phase == worldedit::Phase::Ready) s->worldEdit().start();
            else if (s->worldEdit().progress().terminal()) {
                auto result = s->worldEdit().prepare(client, generation);
                player->displayClientMessage(
                    result ? "Preflight queued; review World operations and execute again." : result.error(),
                    std::nullopt
                );
            }
            break;
        }
        case ActionId::SampleSecondary: {
            auto s = app::accessSession();
            if (s) s->worldEdit().sample(client, true);
            break;
        }
        case ActionId::SignCopy:
        case ActionId::SignCopyBack:
        case ActionId::SignPaste: {
            auto s = app::accessSession();
            if (s)
                player->displayClientMessage(
                    action.id == ActionId::SignPaste
                        ? s->signs().arm()
                        : s->signs().copy(client, s->placement(), action.id == ActionId::SignCopyBack),
                    std::nullopt
                );
            break;
        }
        case ActionId::MainMenu:
            ui::requestOpen(ui::PageId::MainMenu);
            return;
        case ActionId::Settings:
            ui::requestOpen(ui::PageId::Settings);
            return;
        case ActionId::Placements:
            ui::requestOpen(ui::PageId::Projection);
            return;
        case ActionId::PlacementSettings:
            ui::requestOpen(ui::PageId::Placement);
            return;
        case ActionId::SelectionSettings:
        case ActionId::SelectionManager:
            ui::requestOpen(ui::PageId::Selection);
            return;
        case ActionId::SaveSelection:
            ui::requestOpen(ui::PageId::SaveSelection);
            return;
        case ActionId::Verifier:
            ui::requestOpen(ui::PageId::Verifier);
            return;
        case ActionId::Materials:
            ui::requestOpen(ui::PageId::Materials);
            return;
        case ActionId::PickBlock: {
            auto session = app::accessSession();
            if (session) session->survival().pick(client);
            break;
        }
        case ActionId::Info:
        case ActionId::InfoRelease: {
            auto session = app::accessSession();
            if (session) session->survival().info(action.id == ActionId::Info);
            break;
        }
        case ActionId::EasyPlace:
        case ActionId::EasyPlaceRelease: {
            auto session = app::accessSession();
            if (session) session->survival().use(action.id == ActionId::EasyPlace);
            break;
        }
        case ActionId::TogglePickBlock:
        case ActionId::ToggleEasyPlace:
        case ActionId::ToggleRestriction: {
            auto config = app::configStore().snapshot();
            if (action.id == ActionId::TogglePickBlock) config.survival.pickBlock = !config.survival.pickBlock;
            else if (action.id == ActionId::ToggleEasyPlace) config.survival.easyPlace = !config.survival.easyPlace;
            else config.survival.placementRestriction = !config.survival.placementRestriction;
            if (app::configStore().submit(config).empty()) (void)app::configStore().save();
            break;
        }
        case ActionId::NextLayerMode:
        case ActionId::PreviousLayerMode:
            if (mContext.viewService->cycleMode(action.id == ActionId::NextLayerMode ? 1 : -1))
                mContext.refreshProjectionState();
            break;
        case ActionId::SetLayerHere:
            if (mContext.viewService->setAt(BlockPos(player->getPosition()))) mContext.refreshProjectionState();
            break;
        case ActionId::ToggleTool:
            tools->toggle();
            break;
        case ActionId::CycleMode:
        case ActionId::CycleModeReverse:
            tools->cycle(action.id == ActionId::CycleModeReverse ? -1 : 1, player->isCreative());
            break;
        case ActionId::CycleSubmode:
            if (tools->state().activeTool == editor::ToolMode::Paste
                || tools->state().activeTool == editor::ToolMode::GridPaste) {
                auto s = app::accessSession();
                if (s) {
                    auto& policy = s->worldEdit().options().policy;
                    policy       = static_cast<worldedit::ReplacePolicy>((static_cast<int>(policy) + 1) % 3);
                }
            } else tools->cycleSubmode();
            break;
        case ActionId::IncreaseLayer:
        case ActionId::DecreaseLayer: {
            auto const& range = mContext.viewService->currentLayerRange();
            auto const& pos = player->getPosition();
            double coordinate = range.axis == editor::LayerAxis::X ? pos.x
                              : range.axis == editor::LayerAxis::Y ? pos.y : pos.z;
            if (mContext.viewService->step(action.id == ActionId::IncreaseLayer ? 1 : -1, coordinate))
                mContext.refreshProjectionState();
            break;
        }
        default: {
            auto                    start = player->getHeadPos();
            auto                    view  = player->getViewVector(1.0f);
            selection::tool::Vector direction{view.x, view.y, view.z};
            if (editor::creativeTool(tools->state().activeTool) && action.id == ActionId::Select) {
                auto s = app::accessSession();
                if (s) s->worldEdit().sample(client, false);
                break;
            }
            if (tools->state().activeTool == editor::ToolMode::Edit
                && (action.id == ActionId::Select || action.id == ActionId::Primary || action.id == ActionId::Secondary)) {
                (void)tools->editRay({start.x,start.y,start.z}, direction, static_cast<int>(player->getDimensionId()),
                    action.id == ActionId::Select ? 2 : action.id == ActionId::Primary ? 0 : 1);
                break;
            }
            if (action.id == ActionId::Select) {
                tools->select({start.x, start.y, start.z}, direction, static_cast<int>(player->getDimensionId()));
            } else if (action.id == ActionId::NudgePositive || action.id == ActionId::NudgeNegative) {
                if (tools->nudge(
                        direction,
                        action.id == ActionId::NudgePositive ? 1 : -1,
                        static_cast<int>(player->getDimensionId())
                    ))
                    mContext.refreshProjectionState();
            } else {
                Vec3 end{start.x + view.x * 200.f, start.y + view.y * 200.f, start.z + view.z * 200.f};
                auto hit = player->getDimension().getBlockSourceFromMainChunkSource().clip(
                    start,
                    end,
                    false,
                    ShapeType::Outline,
                    200,
                    false,
                    false,
                    player,
                    
                    [](BlockSource const&, Block const&, bool) { return true; },
                    false
                );
                if (hit.mType != HitResultType::Tile) break;
                auto pos      = hit.mBlock;
                bool adjacent = tools->state().activeTool == editor::ToolMode::Selection ? player->isSneaking()
                                                                                         : !player->isSneaking();
                if (adjacent && hit.mFacing < 6) {
                    auto const& face = Facing::DIRECTION()[hit.mFacing];
                    auto        next = selection::tool::offset({pos.x, pos.y, pos.z}, {face.x, face.y, face.z});
                    if (!next) break;
                    pos = {(*next)[0], (*next)[1], (*next)[2]};
                }
                if (tools->click(pos, action.id == ActionId::Secondary, static_cast<int>(player->getDimensionId())))
                    mContext.refreshProjectionState();
            }
            break;
        }
        }
    }
}
} 
