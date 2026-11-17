#pragma once
#include "levischematic/app/lifecycle/AppKernel.h"
#include "ll/api/event/EventBus.h"
namespace levischematic::app {
void                              drainSession() noexcept;
extern SessionGate                gSessionGate;
extern bool                       gStopRequested;
extern bool                       gLeaveRequested;
extern bool                       gDraining;
extern bool                       gReloading;
extern bool                       gReloadQueued;
extern unsigned                   gReloadOperations;
extern std::unique_ptr<AppKernel> gAppKernel;
extern ll::event::ListenerPtr     gJoinListener;
extern ll::event::ListenerPtr     gTickListener;
extern ll::event::ListenerPtr     gSaveTickListener;
extern ll::event::ListenerPtr     gClientCommandListener;
extern ll::event::ListenerPtr     gServerCommandListener;
bool                              resourceLoadingComplete(ClientInstance&);

}
