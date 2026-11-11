#pragma once
#include "SchematicBlockSource.h"
#include "SchematicChunkSource.h"
#include "levischematic/editor/EditorState.h"
#include "levischematic/util/WorkBudget.h"
#include "mc/util/Mirror.h"
#include "mc/util/Rotation.h"
#include <deque>
#include <unordered_set>
namespace levischematic::render {

struct FakeWorld {
    
    struct Entry {
        BlockPos                pos;
        Block const*            block;
        std::optional<BlockPos> source;
    };
    
    struct SubChunk {
        std::deque<Entry> blocks;
        size_t            cursor = 0;
        uint64_t          epoch = UINT64_MAX, viewRevision = 0, stamp = 0;
        BlockPos          visibilityOrigin;
        bool              built = false, ready = false;
    };
    
    FakeWorld(placement::PlacementInstance const&, Dimension&);
    
    void advance(
        placement::PlacementInstance const&,
        verifier::VerifierState const&,
        editor::ViewState const&,
        util::WorkBudget const&
    );
    
    bool ready(BlockPos origin) const;
    
    void                                             markDirty(BlockPos pos);
    BlockPos                                         transformedSize;
    std::unique_ptr<schematic::SchematicChunkSource> chunkSource;
    std::unique_ptr<schematic::SchematicBlockSource> blockSource;
    std::unordered_map<uint64_t, SubChunk>           subChunks;
    std::deque<BlockPos>                             origins;
    std::unordered_set<uint64_t>                     dirty;
    bool                                             indexed = false;

private:
    
    void append(BlockPos, Block const*, std::optional<BlockPos>);
    
    void write(BlockPos, Block const&);
    void updateConnections(BlockPos);
    std::deque<BlockPos> mConnectionQueue;
    std::unordered_set<uint64_t> mQueuedConnections;
    std::unordered_map<uint64_t, size_t> mConnectionPendingChunks;
    
    bool visible(
        placement::PlacementInstance const&,
        verifier::VerifierState const&,
        editor::ViewState const&,
        BlockPos
    ) const;
    
    uint64_t     visibilityStamp(placement::PlacementInstance const&, verifier::VerifierState const&, BlockPos) const;
    int          mMinHeight;
    size_t       mIndex = 0, mBuild = 0, mVisibility = 0;
    std::unordered_map<uint64_t, placement::OverrideEntry> mOverrides;
    std::unordered_map<uint64_t, placement::OverrideEntry>::const_iterator mOverride;
};

inline ::Rotation toMinecraftRotation(placement::PlacementInstance::Rotation rotation) {
    switch (rotation) {
    case placement::PlacementInstance::Rotation::CW_90:
        return ::Rotation::Clockwise90;
    case placement::PlacementInstance::Rotation::CW_180:
        return ::Rotation::Clockwise180;
    case placement::PlacementInstance::Rotation::CCW_90:
        return ::Rotation::CounterClockwise90;
    case placement::PlacementInstance::Rotation::NONE:
        break;
    }
    return ::Rotation::None;
}

inline ::Mirror toMinecraftMirror(placement::PlacementInstance::Mirror mirror) {
    switch (mirror) {
    case placement::PlacementInstance::Mirror::MIRROR_X:
        return ::Mirror::Z; 
    case placement::PlacementInstance::Mirror::MIRROR_Z:
        return ::Mirror::X;
    case placement::PlacementInstance::Mirror::NONE:
        break;
    }
    return ::Mirror::None;
}

BlockPos transformedSizeFor(placement::PlacementInstance const&);

AABB placementRenderBounds(placement::PlacementInstance const&);
} 
