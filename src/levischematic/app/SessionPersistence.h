#pragma once
#include "levischematic/editor/EditorState.h"
#include "levischematic/util/WorkBudget.h"
#include <filesystem>
#include <memory>
#include <string>
class ClientInstance;
namespace levischematic::app {
struct SessionPersistenceStatus {
    bool        available = false, pending = false, restoring = false;
    size_t      placements = 0;
    std::string identity, detail;
};

class SessionPersistence {
public:
    SessionPersistence(editor::EditorState&, placement::PlacementStore&, std::filesystem::path configDirectory);
    ~SessionPersistence();
    void                                   tick(ClientInstance&, util::WorkBudget const&);
    void                                   flush() noexcept;
    void                                   cancelRestore();
    bool                                   requestRestore();
    void                                   keepCurrent();
    [[nodiscard]] SessionPersistenceStatus status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};
} 
