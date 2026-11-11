#pragma once


#include "mc/world/actor/ActorType.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/phys/AABB.h"
#include <cstdint>
#include <unordered_set>

class Actor;
class BlockPos;
class BlockSourceListener;
class ChunkSource;
class Dimension;
class Level;
struct Bounds;

namespace levischematic::schematic {






class SchematicBlockSource : public ::BlockSource {
public:
    AABB mBounds;

    using BlockSource::getBlock;
    Block const& getBlock(BlockPos const& pos) const override;
    Block const& getBlock(BlockPos const& pos, uint layer) const override;
    Block const& getExtraBlock(BlockPos const& pos) const override;
    bool setProjectionVisible(BlockPos const& pos, bool visible);
    bool projectionVisible(BlockPos const& pos) const;

    






    SchematicBlockSource(::Level& level, ::Dimension& dimension, ::ChunkSource& chunkSource, ::AABB const& bounds);

    


    virtual ~SchematicBlockSource() override = default;

    



    virtual void addListener(::BlockSourceListener&) override {};

    



    virtual void removeListener(::BlockSourceListener&) override {};

    






    virtual ::gsl::span<::gsl::not_null<::Actor*>>
    fetchEntities(::Actor const* except, ::AABB const& bb, bool useHitbox, bool getDisplayEntities) override;

    






    virtual ::gsl::span<::gsl::not_null<::Actor*>> fetchEntities(
        ::ActorType                     entityTypeId,
        ::AABB const&                   bb,
        ::Actor const*                  except,
        ::std::function<bool(::Actor*)> selector
    ) override;

    




    virtual bool hasChunksAt(::Bounds const&, bool) const override {
        return true;
    };

    





    virtual bool hasChunksAt(::BlockPos const&, int, bool) const override {
        return true;
    };

    




    virtual bool hasChunksAt(::AABB const&, bool) const override {
        return true;
    };
private:
    std::unordered_set<uint64_t> mHiddenPositions;
    Block const* mAir = nullptr;
};
} 
