#include "levischematic/presentation/ui/pages/common/PageCommon.h"
#include "levischematic/presentation/ui/i18n/UiI18n.h"
#include "levischematic/shared/i18n/I18n.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include <charconv>
#include <ctime>

namespace levischematic::ui {
bool parseInt(std::string const& text, int& value) {
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}
std::optional<BlockPos> playerBlockPos() {
    auto client = ll::service::getClientInstance();
    auto player = client ? client->getLocalPlayer() : nullptr;
    if (!player) return {};
    return BlockPos(player->getPosition());
}
int playerDimension() {
    auto client = ll::service::getClientInstance();
    auto player = client ? client->getLocalPlayer() : nullptr;
    return player ? static_cast<int>(player->getDimensionId()) : 0;
}
std::string utf8Path(std::filesystem::path const& path) {
    auto value = path.u8string();
    return {reinterpret_cast<char const*>(value.data()), value.size()};
}
std::filesystem::path pathFromUtf8(std::string const& text) {
    return std::filesystem::path(std::u8string_view{reinterpret_cast<char8_t const*>(text.data()), text.size()});
}
void mainMenuButton(PageBuilder& b) {
    b.button("main_menu", tr("ui.common.main_menu"), [] { requestOpen(PageId::MainMenu); }).in(Slot::FooterRight);
}
void navButton(PageBuilder& b, std::string id, std::string label, PageId target, Slot slot, IconRef icon) {
    b.button(std::move(id), std::move(label), [target] { requestOpen(target); }).in(slot).icon(icon);
}
void confirm(std::shared_ptr<Page> const& p, std::string message, std::function<void()> action) {
    beginModal(p, std::move(message));
    PageBuilder{p}
        .button(
            "confirm",
            tr("ui.common.confirm"),
            [p, action = std::move(action)] {
                dismissModal(p);
                action();
            }
        )
        .button("cancel", tr("ui.common.cancel"), [p] { dismissModal(p); });
}
void prompt(
    std::shared_ptr<Page> const&     p,
    std::string                      message,
    std::string                      initial,
    std::function<void(std::string)> action
) {
    beginModal(p, std::move(message));
    PageBuilder b{p};
    b.input("prompt_text", {}, std::move(initial), [](std::string) {});
    b.button("confirm", tr("ui.common.ok"), [p, action = std::move(action)] {
        std::string text;
        if (auto row = findRow(*p, "prompt_text")) text = row->value;
        dismissModal(p);
        action(std::move(text));
    });
    b.button("cancel", tr("ui.common.cancel"), [p] { dismissModal(p); });
}
std::string formatTime(int64_t milliseconds) {
    if (milliseconds <= 0) return {};
    std::time_t seconds = static_cast<std::time_t>(milliseconds / 1000);
    std::tm     local{};
    if (localtime_s(&local, &seconds) != 0) return {};
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    return buffer;
}
} // namespace levischematic::ui
