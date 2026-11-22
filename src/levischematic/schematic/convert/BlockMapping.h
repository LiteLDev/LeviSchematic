#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace levischematic::convert {
using BedrockStateValue = std::variant<int32_t, bool, std::string>;

struct BedrockBlockState {
    std::string                                            name;
    std::vector<std::pair<std::string, BedrockStateValue>> states;
};
std::string bedrockStateKey(BedrockBlockState const& state);

enum class MatchQuality {
    Exact,
    Fallback,
    Unmapped,
};

struct JavaToBedrock {
    BedrockBlockState block;
    bool              waterlogged = false;
    MatchQuality      quality     = MatchQuality::Exact;
};
struct BedrockToJava {
    std::string  state;
    MatchQuality quality = MatchQuality::Exact;
};

class BlockMapping {
public:
    static BlockMapping parse(std::string_view json);

    [[nodiscard]] JavaToBedrock toBedrock(std::string_view javaState) const;
    [[nodiscard]] BedrockToJava toJava(BedrockBlockState const& block, bool waterlogged) const;

    [[nodiscard]] int32_t            javaDataVersion() const { return mJavaDataVersion; }
    [[nodiscard]] std::string const& javaVersion() const { return mJavaVersion; }
    [[nodiscard]] std::string const& bedrockVersion() const { return mBedrockVersion; }
    [[nodiscard]] size_t             size() const { return mJavaToBedrock.size(); }

private:
    int32_t                                      mJavaDataVersion = 0;
    std::string                                  mJavaVersion;
    std::string                                  mBedrockVersion;
    std::vector<BedrockBlockState>               mBedrock;
    std::unordered_map<std::string, uint32_t>    mJavaToBedrock;
    std::unordered_map<std::string, std::string> mJavaDefaults;
    std::unordered_map<std::string, std::string> mBedrockToJava;
    std::unordered_map<std::string, std::string> mBedrockNameToJava;
};
} // namespace levischematic::convert
