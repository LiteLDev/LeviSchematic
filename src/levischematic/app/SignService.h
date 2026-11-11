#pragma once
#include <atomic>
#include <optional>
#include <string>
class IClientInstance;
namespace levischematic::app {
class PlacementService;
class SignService {
    std::string mText, mDetail;
    bool        mArmed = false, mHasText = false;

public:
    inline static std::atomic_bool editorAvailable{false};
    std::string                    copy(IClientInstance&, PlacementService&, bool back);
    std::string                    arm();
    void                           cancel() { mArmed = false; }
    std::optional<std::string>     consume();
    std::string const&             text() const { return mText; }
    std::string const&             detail() const { return mDetail; }
};
} 
