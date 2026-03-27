#include "PlacementProjectionCache.h"

#include "levishematic/util/PositionUtils.h"

namespace levishematic::placement {

PlacementProjectionCache::View PlacementProjectionCache::view(PlacementInstance const& placement) {
    auto const& record = ensureRecord(placement);
    return View{record.worldEntries, record.byPos, record.bySubChunk};
}

void PlacementProjectionCache::clear() {
    mRecords.clear();
}

void PlacementProjectionCache::remove(PlacementId id) {
    mRecords.erase(id);
}

PlacementProjectionCache::Record const&
PlacementProjectionCache::ensureRecord(PlacementInstance const& placement) {
    auto& record = mRecords[placement.id];
    if (record.revision != placement.revision) {
        record = buildRecord(placement);
    }
    return record;
}

PlacementProjectionCache::Record PlacementProjectionCache::buildRecord(PlacementInstance const& placement) {
    Record record;
    record.revision = placement.revision;

    if (!placement.asset) {
        return record;
    }

    record.byPos.reserve(placement.asset->localBlocks.size() + placement.overrides.size());

    auto applyEntry = [&](render::ProjEntry entry) {
        auto posKey          = util::encodePosKey(entry.pos);
        record.byPos[posKey] = entry;
    };

    for (auto const& localEntry : placement.asset->localBlocks) {
        if (!localEntry.block || localEntry.block->isAir()) {
            continue;
        }

        auto local = transformLocalPos(
            localEntry.localPos,
            placement.asset->size,
            placement.mirror,
            placement.rotation
        );
        render::ProjEntry resolved{
            BlockPos{
                placement.origin.x + local.x,
                placement.origin.y + local.y,
                placement.origin.z + local.z,
            },
            localEntry.block,
            render::kDefaultProjectionColor,
        };

        auto posKey = util::encodePosKey(resolved.pos);
        if (auto overrideIt = placement.overrides.find(posKey); overrideIt != placement.overrides.end()) {
            if (overrideIt->second.kind == OverrideEntry::Kind::Remove) {
                record.byPos.erase(posKey);
                continue;
            }
            resolved.block = overrideIt->second.block;
        }

        applyEntry(resolved);
    }

    for (auto const& [posKey, overrideEntry] : placement.overrides) {
        if (overrideEntry.kind != OverrideEntry::Kind::SetBlock || overrideEntry.block == nullptr) {
            continue;
        }

        applyEntry(render::ProjEntry{
            util::decodePosKey(posKey),
            overrideEntry.block,
            render::kDefaultProjectionColor,
        });
    }

    record.worldEntries.reserve(record.byPos.size());
    for (auto const& [posKey, entry] : record.byPos) {
        (void)posKey;
        record.worldEntries.push_back(entry);
        auto subChunkKey = util::subChunkKeyFromWorldPos(entry.pos.x, entry.pos.y, entry.pos.z);
        record.bySubChunk[subChunkKey].push_back(entry);
    }

    return record;
}

} // namespace levishematic::placement
