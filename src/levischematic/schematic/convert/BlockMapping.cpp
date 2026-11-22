#include "levischematic/schematic/convert/BlockMapping.h"
#include "levischematic/schematic/convert/LitematicDocument.h"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace levischematic::convert {
namespace {
constexpr std::string_view Air = "minecraft:air";

std::string withNamespace(std::string_view name) {
    return name.find(':') == std::string_view::npos ? "minecraft:" + std::string(name) : std::string(name);
}
std::string javaBlockName(std::string_view state) { return std::string(state.substr(0, state.find('['))); }
bool        hasProperty(std::string_view state, std::string_view property) {
    auto open = state.find('[');
    if (open == std::string_view::npos) return false;
    auto body = state.substr(open + 1);
    for (size_t pos = 0; (pos = body.find(property, pos)) != std::string_view::npos; pos += property.size())
        if (pos == 0 || body[pos - 1] == ',') return true;
    return false;
}
int preference(std::string const& state, std::unordered_map<std::string, std::string> const& defaults) {
    auto it     = defaults.find(javaBlockName(state));
    int  score  = it != defaults.end() && it->second == state ? 2 : 0;
    score      += hasProperty(state, "waterlogged=true") ? 0 : 1;
    return score;
}
void preferInsert(
    std::unordered_map<std::string, std::string>&       map,
    std::string const&                                  key,
    std::string const&                                  state,
    std::unordered_map<std::string, std::string> const& defaults
) {
    auto [it, inserted] = map.try_emplace(key, state);
    if (!inserted && preference(state, defaults) > preference(it->second, defaults)) it->second = state;
}
} // namespace

std::string bedrockStateKey(BedrockBlockState const& state) {
    auto states = state.states;
    std::sort(states.begin(), states.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    std::string key  = withNamespace(state.name);
    key             += '{';
    for (auto const& [name, value] : states) {
        key += name;
        key += '=';
        if (auto const* b = std::get_if<bool>(&value)) key += *b ? "1b" : "0b";
        else if (auto const* i = std::get_if<int32_t>(&value)) key += std::to_string(*i);
        else key += '"' + std::get<std::string>(value) + '"';
        key += ';';
    }
    key += '}';
    return key;
}

BlockMapping BlockMapping::parse(std::string_view json) {
    BlockMapping result;
    try {
        auto document = nlohmann::json::parse(json);
        if (document.at("format").get<int>() != 1) throw std::runtime_error("unsupported mapping format");
        result.mJavaDataVersion = document.at("java_data_version").get<int32_t>();
        result.mJavaVersion     = document.at("java_version").get<std::string>();
        result.mBedrockVersion  = document.at("bedrock_version").get<std::string>();
        for (auto const& [name, properties] : document.at("java_defaults").items()) {
            auto block                  = withNamespace(name);
            auto props                  = properties.get<std::string>();
            result.mJavaDefaults[block] = props.empty() ? block : block + "[" + props + "]";
        }
        std::unordered_map<std::string, uint32_t> bedrockIndex;
        auto const&                               blocks = document.at("blocks");
        result.mJavaToBedrock.reserve(blocks.size());
        for (auto const& entry : blocks) {
            auto              java = withNamespace(entry.at(0).get<std::string>());
            auto              name = entry.at(1).get<std::string>();
            BedrockBlockState bedrock;
            bedrock.name = withNamespace(name.empty() ? javaBlockName(java) : name);
            for (auto const& [key, value] : entry.at(2).items()) {
                if (value.is_boolean()) bedrock.states.emplace_back(key, value.get<bool>());
                else if (value.is_number_integer()) bedrock.states.emplace_back(key, value.get<int32_t>());
                else if (value.is_string()) bedrock.states.emplace_back(key, value.get<std::string>());
                else throw std::runtime_error("invalid Bedrock state value");
            }
            std::sort(bedrock.states.begin(), bedrock.states.end(), [](auto const& a, auto const& b) {
                return a.first < b.first;
            });
            auto key            = bedrockStateKey(bedrock);
            auto [it, inserted] = bedrockIndex.try_emplace(key, static_cast<uint32_t>(result.mBedrock.size()));
            if (inserted) result.mBedrock.push_back(bedrock);
            result.mJavaToBedrock[java] = it->second;
            preferInsert(result.mBedrockToJava, key, java, result.mJavaDefaults);
            preferInsert(result.mBedrockNameToJava, bedrock.name, java, result.mJavaDefaults);
        }
    } catch (nlohmann::json::exception const& e) {
        throw std::runtime_error(std::string("Block mapping: ") + e.what());
    } catch (std::runtime_error const& e) {
        throw std::runtime_error(std::string("Block mapping: ") + e.what());
    }
    if (!result.mJavaToBedrock.contains(std::string(Air))) throw std::runtime_error("Block mapping: air is missing.");
    return result;
}

JavaToBedrock BlockMapping::toBedrock(std::string_view javaState) const {
    auto [name, properties] = splitJavaState(javaState);
    auto          key       = canonicalJavaState(name, properties);
    JavaToBedrock result;
    for (auto const& [property, value] : properties)
        if (property == "waterlogged") result.waterlogged = value == "true";
    auto it = mJavaToBedrock.find(key);
    if (it == mJavaToBedrock.end()) {
        auto fallback = mJavaDefaults.find(withNamespace(name));
        if (fallback != mJavaDefaults.end()) {
            it             = mJavaToBedrock.find(fallback->second);
            result.quality = MatchQuality::Fallback;
        }
    }
    if (it == mJavaToBedrock.end()) {
        it                 = mJavaToBedrock.find(std::string(Air));
        result.quality     = MatchQuality::Unmapped;
        result.waterlogged = false;
    }
    result.block = mBedrock[it->second];
    return result;
}

BedrockToJava BlockMapping::toJava(BedrockBlockState const& block, bool waterlogged) const {
    BedrockToJava result;
    if (auto it = mBedrockToJava.find(bedrockStateKey(block)); it != mBedrockToJava.end()) result.state = it->second;
    else if (auto byName = mBedrockNameToJava.find(withNamespace(block.name)); byName != mBedrockNameToJava.end()) {
        result.state   = byName->second;
        result.quality = MatchQuality::Fallback;
    } else {
        result.state   = std::string(Air);
        result.quality = MatchQuality::Unmapped;
        return result;
    }
    if (waterlogged && hasProperty(result.state, "waterlogged=false")) {
        auto [name, properties] = splitJavaState(result.state);
        for (auto& [property, value] : properties)
            if (property == "waterlogged") value = "true";
        auto candidate = canonicalJavaState(name, std::move(properties));
        if (mJavaToBedrock.contains(candidate)) result.state = std::move(candidate);
    }
    return result;
}
} // namespace levischematic::convert
