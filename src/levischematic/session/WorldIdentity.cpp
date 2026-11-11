#include "WorldIdentity.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/ServerInfo.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/network/GameConnectionInfo.h"
#include <filesystem>
namespace levischematic::session {
std::string worldIdentity(ClientInstance& client) {
    if (ll::service::getServerInstance()) {
        auto path = ll::getWorldPath();
        if (!path) return {};
        auto value = std::filesystem::weakly_canonical(*path).generic_u8string();
        return "local:" + std::string(reinterpret_cast<char const*>(value.data()), value.size());
    }
    auto connection = client.getGameConnectionInfo();
    if (!connection) return {};
    auto host = connection->mUnresolvedUrl.get();
    if (host.empty()) host = connection->mHostIpAddress.get();
    for (auto& c : host)
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (host.empty() || connection->mPort <= 0) return {};
    return "server:" + host + ":" + std::to_string(connection->mPort);
}
} 
