#pragma once

namespace levischematic::command {

// Registers the `/schem` command tree for either server or client contexts.
void registerCommands(bool isClient);

} // namespace levischematic::command
