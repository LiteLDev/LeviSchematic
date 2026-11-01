#include "VanillaChunkInstanceSet.h"
#include "ChunkLightingCapabilities.h"


#include "levischematic/LeviSchematic.h"


#include "mc/client/renderer/block/BakedBlockLightType.h"
#include "mc/client/renderer/chunks/RenderChunkBuilder.h"
#include "mc/client/renderer/chunks/RenderChunkGeometry.h"
#include "mc/deps/core/utility/buffer_span.h"
#include "mc/world/level/SubChunkPos.h"

#include <cmath>
#include <cstdint>
#include <new>
#include <utility>

namespace levischematic::render {
namespace {


auto& getLogger() { return levischematic::LeviSchematic::getInstance().getSelf().getLogger(); }


[[nodiscard]] constexpr int signOf(int value) noexcept { return (value > 0) - (value < 0); }


constexpr bool kKeepAllFaceDirectionsForPreview = true;


[[nodiscard]] std::size_t releaseRenderChunkGeometry(std::shared_ptr<::RenderChunkGeometry> const& geometry) {
    auto* const raw = geometry.get();
    if (raw == nullptr) {
        return 0;
    }
    raw->reset();
    raw->mEntityId.get().mRawId = static_cast<::EntityId::entity_type>(-1);
    return 1;
}


void writeSortCameraDelta(
    SchematicRenderChunkInstanced& instanced,
    ::BlockPos const&              position,
    ::Vec3 const&                  sortCamera
) noexcept {
    auto const cameraX                      = static_cast<int>(std::floor(sortCamera.x));
    auto const cameraY                      = static_cast<int>(std::floor(sortCamera.y));
    auto const cameraZ                      = static_cast<int>(std::floor(sortCamera.z));
    instanced.mLastSortCameraDelta.get()[0] = signOf((position.x >> 4) - (cameraX >> 4));
    instanced.mLastSortCameraDelta.get()[1] = signOf((position.y >> 4) - (cameraY >> 4));
    instanced.mLastSortCameraDelta.get()[2] = signOf((position.z >> 4) - (cameraZ >> 4));
}


void logSortCameraDelta(::BlockPos const& position, ::glm::ivec3 const& cameraDelta) {
    static std::string lastMessage;
    std::string const  message = "VanillaChunkInstanceSet: sorted chunk=(" + std::to_string(position.x) + ","
                               + std::to_string(position.y) + "," + std::to_string(position.z) + ") chunkPos=("
                               + std::to_string(position.x >> 4) + "," + std::to_string(position.y >> 4) + ","
                               + std::to_string(position.z >> 4) + ") cameraDelta=(" + std::to_string(cameraDelta[0])
                               + "," + std::to_string(cameraDelta[1]) + "," + std::to_string(cameraDelta[2]) + ")"
                               + (kKeepAllFaceDirectionsForPreview ? " faceCulling=forcedAll" : " faceCulling=vanilla");
    if (message == lastMessage) {
        return;
    }
    lastMessage = message;
    getLogger().debug("{}", message);
}

} 

void VanillaChunkInstanceSet::reset() {
    clear();

    (void)createSorter();
}

void VanillaChunkInstanceSet::clear() {
    mDirty.clear();


    std::size_t releasedGeometries = 0;
    for (auto const& [chunkOrigin, instanced] : mInstances) {
        if (!instanced) {
            continue;
        }
        releasedGeometries += releaseRenderChunkGeometry(instanced->mSortingRenderChunkGeometry);
        releasedGeometries += releaseRenderChunkGeometry(instanced->mNextRenderChunkGeometry);
        releasedGeometries += releaseRenderChunkGeometry(instanced->mCurrentRenderChunkGeometry);
        if (instanced->mRenderChunkShared) {
            releasedGeometries +=
                releaseRenderChunkGeometry(instanced->mRenderChunkShared->mCurrentRenderChunkGeometry);
            releasedGeometries +=
                releaseRenderChunkGeometry(instanced->mRenderChunkShared->mBuildingRenderChunkGeometry);
        }
    }
    if (!mInstances.empty()) {
        getLogger().debug(
            "VanillaChunkInstanceSet: releasing {} chunk instance(s) with {} geometry(ies) before destruction",
            mInstances.size(),
            releasedGeometries
        );
    }

    mInstances.clear();
    mSorter.reset();
}

void VanillaChunkInstanceSet::invalidate(BlockPos const& origin) {
    if (mInstances.contains(origin)) mDirty.insert(origin);
}

bool VanillaChunkInstanceSet::needsRebuild(BlockPos const& origin) const {
    auto it = mInstances.find(origin);
    return mDirty.contains(origin) || it == mInstances.end() || !it->second->mRenderChunkShared
        || !it->second->mRenderChunkShared->mCurrentRenderChunkGeometry;
}

bool VanillaChunkInstanceSet::ready() const noexcept { return static_cast<bool>(mSorter); }

bool VanillaChunkInstanceSet::createSorter() {
    auto sorter = std::make_unique<SchematicRenderChunkSorter>(std::shared_ptr<SchematicRenderChunkSorterSharedInfo>{});


    sorter->mForceDisableCulling = kKeepAllFaceDirectionsForPreview;


    getLogger().debug(
        "VanillaChunkInstanceSet: RenderChunkSorter created at {:p} (sharedInfo={:p}, "
        "forceDisableCulling={})",
        static_cast<void*>(sorter.get()),
        static_cast<void*>(sorter->mSharedInfo.get()),
        sorter->mForceDisableCulling
    );

    mSorter = std::move(sorter);
    return true;
}

SchematicRenderChunkInstanced* VanillaChunkInstanceSet::obtain(::BlockPos const& chunkOrigin) {
    auto const existing = mInstances.find(chunkOrigin);
    if (existing != mInstances.end()) {
        return existing->second.get();
    }
    auto instanced = std::make_unique<SchematicRenderChunkInstanced>();


    ::SubChunkPos const chunkPos{chunkOrigin.x >> 4, chunkOrigin.y >> 4, chunkOrigin.z >> 4};
    instanced->mRenderChunkShared = makeSchematicRenderChunkShared(chunkPos);

    auto const inserted = mInstances.emplace(chunkOrigin, std::move(instanced));
    if (inserted.second) {

        auto const& shared = inserted.first->second->mRenderChunkShared;
        getLogger().debug(
            "VanillaChunkInstanceSet: RenderChunkInstanced created at {:p} for chunk=({}, {}, {}), "
            "RenderChunkShared={:p} (position={}/{}/{})",
            static_cast<void*>(inserted.first->second.get()),
            chunkOrigin.x,
            chunkOrigin.y,
            chunkOrigin.z,
            static_cast<void*>(shared.get()),
            shared ? shared->mPosition->x : 0,
            shared ? shared->mPosition->y : 0,
            shared ? shared->mPosition->z : 0
        );
    }
    return inserted.first->second.get();
}

bool VanillaChunkInstanceSet::prepareRebuild(SchematicRenderChunkInstanced& instanced, RenderChunkBuilder& builder) {
    auto& shared = instanced.mRenderChunkShared;
    if (!shared) {
        return false;
    }


    Vec3 const currentCameraPosition{0.0f, 0.0f, 0.0f};
    shared->startRebuild(builder, currentCameraPosition);

    auto& building = shared->mBuildingRenderChunkGeometry;
    if (!building) {
        return false;
    }

    mDirty.erase(shared->mPosition.get());
    return true;
}

void VanillaChunkInstanceSet::buildPrepared(SchematicRenderChunkInstanced& instanced, RenderChunkBuilder& builder) {
    detail::ChunkLightingCapabilities const lighting;
    instanced.mRenderChunkShared->mBuildingRenderChunkGeometry
        ->rebuild(builder, false, ::BakedBlockLightType::Flat, false, lighting.value);
}

void VanillaChunkInstanceSet::finishRebuild(
    SchematicRenderChunkInstanced& instanced,
    RenderChunkBuilder&            builder,
    ::mce::BufferResourceService&  bufferResourceService
) {
    auto& shared   = instanced.mRenderChunkShared;
    auto& building = shared->mBuildingRenderChunkGeometry;
    building->endRebuild(
        builder,
        bufferResourceService,
        shared->mBuildState->load() == SchematicRenderChunkShared::BuildState::Building,
        shared->mCurrentRenderChunkGeometry != nullptr,
        shared->mRenderMetadata,
        false
    );
    shared->mBuildState->store(SchematicRenderChunkShared::BuildState::NotBuldingOrSorting);


    shared->mCurrentRenderChunkGeometry = std::move(building);
}

bool VanillaChunkInstanceSet::prepareSort(SchematicRenderChunkInstanced& instanced, ::Vec3 const& sortCamera) {
    if (!mSorter) {
        return false;
    }
    auto& shared = instanced.mRenderChunkShared;
    if (!shared) {
        return false;
    }


    auto const& position = shared->mPosition.get();
    writeSortCameraDelta(instanced, position, sortCamera);


    if (auto& info = mSorter->mSharedInfo) {
        info->x->store(sortCamera.x);
        info->y->store(sortCamera.y);
        info->z->store(sortCamera.z);
        info->dx->store(0.0f);
        info->dy->store(0.0f);
        info->dz->store(0.0f);
        info->cameraIsOrthographic->store(false);
    }

    instanced._startSortCommon(*mSorter);

    return instanced.mSortingRenderChunkGeometry != nullptr;
}

void VanillaChunkInstanceSet::sortPrepared(SchematicRenderChunkInstanced& instanced) {

    auto* sortingGeometry = instanced.mSortingRenderChunkGeometry.get();
    if (sortingGeometry == nullptr) {
        return;
    }


    ::BlockPos const&   geometryPosition = sortingGeometry->mPosition;
    ::SubChunkPos const renderChunkPos{geometryPosition.x >> 4, geometryPosition.y >> 4, geometryPosition.z >> 4};

    auto&                                                     faces = sortingGeometry->mFaceSortingMetaData.get();
    ::buffer_span<std::shared_ptr<::FaceSortingMetaDataBase>> faceSortingData;
    faceSortingData.mBegin = faces.data();
    faceSortingData.mEnd   = faces.data() + faces.size();

    mSorter->sortAndCullFaces(renderChunkPos, faceSortingData);
}

void VanillaChunkInstanceSet::finishSort(
    SchematicRenderChunkInstanced& instanced,
    ::mce::BufferResourceService&  bufferResourceService
) {
    instanced.endSort(*mSorter, bufferResourceService);
    instanced.promoteSortedGeometry();
    logSortCameraDelta(instanced.mRenderChunkShared->mPosition, mSorter->mCameraDelta.get());
}

bool VanillaChunkInstanceSet::sort(
    SchematicRenderChunkInstanced& instanced,
    ::Vec3 const&                  sortCamera,
    ::mce::BufferResourceService&  bufferResourceService
) {
    if (!prepareSort(instanced, sortCamera)) return false;
    sortPrepared(instanced);
    finishSort(instanced, bufferResourceService);
    return true;
}

bool VanillaChunkInstanceSet::cameraDeltaChanged(
    SchematicRenderChunkInstanced const& instanced,
    ::Vec3 const&                        sortCamera
) const noexcept {
    auto const cameraX = static_cast<int>(std::floor(sortCamera.x));
    auto const cameraY = static_cast<int>(std::floor(sortCamera.y));
    auto const cameraZ = static_cast<int>(std::floor(sortCamera.z));
    if (!instanced.mRenderChunkShared) {
        return false;
    }
    auto const& position = instanced.mRenderChunkShared->mPosition.get();
    return instanced.mLastSortCameraDelta.get()[0] != signOf((position.x >> 4) - (cameraX >> 4))
        || instanced.mLastSortCameraDelta.get()[1] != signOf((position.y >> 4) - (cameraY >> 4))
        || instanced.mLastSortCameraDelta.get()[2] != signOf((position.z >> 4) - (cameraZ >> 4));
}

} 
