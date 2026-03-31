#pragma once

namespace levishematic::command {

// Registers the `/schem` command tree for either server or client contexts.
void registerCommands(bool isClient);

} // namespace levishematic::command
