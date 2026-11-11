#include "levischematic/schematic/SchematicChunkSource.h"

#include "levischematic/LeviSchematic.h"

#include "mc/world/level/ILevel.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/deps/core/utility/EnableNonOwnerReferences.h"
#include "mc/world/level/chunk/ChunkSource.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/chunk/LevelChunk.h"

#include <memory>




ChunkSource::ChunkSource()=default;



Bedrock::PubSub::Subscription::Subscription() = default;


static_assert(sizeof(::ChunkSource) == 0x70, "ChunkSource 布局已变化，需重新核对 IDB");

namespace levischematic::schematic {

namespace {




auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }

} 

SchematicChunkSource::SchematicChunkSource(
    ::Dimension&                                   dimension,
    int                                            side,
    ::std::vector<::std::unique_ptr<::LevelChunk>> levelChunks
) {
    
    
    
    
    
    
    
    mControlBlock           = ::std::make_shared<::Bedrock::EnableNonOwnerReferences::ControlBlock>();
    mControlBlock->mIsValid = true;
    getLogger().debug(
        "SchematicChunkSource {} constructed: controlBlock={:p} (mIsValid={}), side={}, chunks={}",
        static_cast<void*>(this),
        static_cast<void*>(mControlBlock.get()),
        mControlBlock->mIsValid,
        side,
        levelChunks.size()
    );

    mChunkSide = side;
    mLevel     = dimension.mLevel.asLevel();
    mDimension = &dimension;
    mParent    = nullptr;
    mOwnedParent  = nullptr;
    mLevelChunkBuilderData = dimension.mLevelChunkBuilderData.get();
    mShuttingDown = false;
    for (auto& item : levelChunks) {
        if (!item) continue;
        auto cp = item->mPosition.get();
        auto chunk = std::shared_ptr<LevelChunk>(std::move(item));
        mChunksByPosition[(static_cast<uint64_t>(static_cast<uint32_t>(cp.x)) << 32) | static_cast<uint32_t>(cp.z)] = chunk;
        mStructureLevelChunks.push_back(std::move(chunk));
    }
}

SchematicChunkSource::~SchematicChunkSource() {
    
    
    getLogger().debug(
        "SchematicChunkSource {} destroying: controlBlock={:p} (mIsValid={}), chunks={}",
        static_cast<void*>(this),
        static_cast<void*>(mControlBlock.get()),
        mControlBlock ? mControlBlock->mIsValid : false,
        mStructureLevelChunks.size()
    );
}

bool SchematicChunkSource::canCreateViews() const { return true; }

std::shared_ptr<::LevelChunk>
SchematicChunkSource::getOrLoadChunk(::ChunkPos const& cp, ::ChunkSource::LoadMode lm, bool readOnly) {
    std::shared_ptr<LevelChunk> result = getExistingChunk(cp);
    if (!result) {
        result = createNewChunk(cp, lm, readOnly);
    }
    return result;
};

bool SchematicChunkSource::isChunkKnown(::ChunkPos const& chunkPos) { return getExistingChunk(chunkPos) != nullptr; };

std::shared_ptr<::LevelChunk>
SchematicChunkSource::createNewChunk(::ChunkPos const& cp, ::ChunkSource::LoadMode, bool readOnly) {
    std::unique_ptr<LevelChunk> newChunk =
        LevelChunk::createNewNoCustomDeleter(*mDimension, cp, readOnly, SubChunkInitMode::ReplaceWithAllAir);

    if (!newChunk) return {};
    newChunk->tryChangeState(newChunk->mLoadState.get(), ChunkState::Loaded);

    std::shared_ptr<LevelChunk> result;
    if (newChunk) {
        result = std::shared_ptr<LevelChunk>(std::move(newChunk));
        mStructureLevelChunks.push_back(result);
        mChunksByPosition[(static_cast<uint64_t>(static_cast<uint32_t>(cp.x)) << 32) | static_cast<uint32_t>(cp.z)] = result;
    }
    return result;
};

std::shared_ptr<::LevelChunk> SchematicChunkSource::getExistingChunk(::ChunkPos const& cp) {
    auto key = (static_cast<uint64_t>(static_cast<uint32_t>(cp.x)) << 32) | static_cast<uint32_t>(cp.z);
    auto it = mChunksByPosition.find(key);
    return it == mChunksByPosition.end() ? nullptr : it->second;
};

} 
