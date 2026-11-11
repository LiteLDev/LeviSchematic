#include "EntityVisualState.h"
#include "mc/deps/nbt/CompoundTag.h"
#include "mc/deps/nbt/ByteTag.h"
#include "mc/entity/components/TintColorComponent.h"
#include <array>
#include <optional>
#include "mc/deps/nbt/IntTag.h"
#include "mc/deps/nbt/ListTag.h"
#include "mc/deps/nbt/StringTag.h"
#include "mc/deps/shared_types/legacy/actor/ArmorSlot.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorDataIDs.h"
#include "mc/world/actor/SynchedActorDataEntityWrapper.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"
namespace levischematic::render {
void applyEntityVisualState(Actor& actor, CompoundTag const& tag) {
    std::vector<std::unique_ptr<DataItem>> data;
    std::optional<mce::Color> tintColor;
    auto integer = [&](CompoundTag const& source, std::string_view key, ActorDataIDs id) {
        if (source.contains(key, Tag::Type::Int) && actor.mEntityData->hasData(static_cast<ushort>(id)))
            data.push_back(DataItem::create(id, source.at(key).get<IntTag>().data));
    };
    integer(tag, "Variant", ActorDataIDs::Variant);
    integer(tag, "MarkVariant", ActorDataIDs::MarkVariant);
    integer(tag, "SkinID", ActorDataIDs::SkinId);
    for (auto [key, id] : {std::pair{"Color", ActorDataIDs::ColorIndex}, {"Color2", ActorDataIDs::Color2Index}}) {
        if (!tag.contains(key, Tag::Type::Byte) || !actor.mEntityData->hasData(static_cast<ushort>(id))) continue;
        auto color = static_cast<uchar>(tag.at(key).get<ByteTag>().data) & 15;
        data.push_back(DataItem::create(id, static_cast<uchar>(color)));
        if (id == ActorDataIDs::ColorIndex
            && actor.getActorIdentifier().mCanonicalName->getString() == "minecraft:sheep") {
            
            
            
            static constexpr std::array<std::array<int, 3>, 16> rgb{{
                {255,255,255}, {249,128,29}, {199,78,189}, {58,179,218},
                {254,216,61}, {128,199,31}, {243,139,170}, {71,79,82},
                {157,157,151}, {22,156,156}, {137,50,184}, {60,68,170},
                {131,84,50}, {94,124,22}, {176,46,38}, {29,29,33}
            }};
            auto c = rgb[color];
            tintColor = mce::Color{c[0] / 255.f, c[1] / 255.f, c[2] / 255.f, color == 0 ? 1.f : 0.f};
        }
    }
    if (tag.contains("Pose", Tag::Type::Compound))
        integer(tag.at("Pose").get<CompoundTag>(), "PoseIndex", ActorDataIDs::PoseIndex);
    if (!data.empty()) actor.mEntityData->assignValues(data, actor, std::nullopt);
    if (tintColor) actor.getEntityContext().getOrAddComponent<TintColorComponent>().mTintColor = *tintColor;
    if (tag.contains("CustomName", Tag::Type::String)) actor.setNameTag(tag.at("CustomName").get<StringTag>());
    if (tag.contains("Armor", Tag::Type::List)) {
        auto const& armor = tag.at("Armor").get<ListTag>();
        for (size_t i = 0; i < std::min<size_t>(4, armor.size()); ++i)
            actor.setArmor(
                static_cast<SharedTypes::Legacy::ArmorSlot>(i),
                ItemStack::fromTag(armor[i].get<CompoundTag>(), actor.getLevel())
            );
    }
    for (auto key : {"Mainhand", "Offhand"}) {
        if (!tag.contains(key, Tag::Type::List)) continue;
        auto const& items = tag.at(key).get<ListTag>();
        if (items.size() == 0) continue;
        auto item = ItemStack::fromTag(items[0].get<CompoundTag>(), actor.getLevel());
        if (std::string_view(key) == "Mainhand") actor.setCarriedItem(item);
        else actor.setOffhandSlot(item);
    }
}
} 
