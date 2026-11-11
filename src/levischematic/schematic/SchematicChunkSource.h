#pragma once

#include "mc/world/level/chunk/ChunkSource.h"
#include <unordered_map>

class ChunkPos;
class Dimension;
class LevelChunk;

namespace levischematic::schematic {






class SchematicChunkSource : public ::ChunkSource {
public:
    std::vector<::std::shared_ptr<::LevelChunk>> mStructureLevelChunks;
    std::unordered_map<uint64_t, std::shared_ptr<LevelChunk>> mChunksByPosition;


    





    SchematicChunkSource(::Dimension& dimension, int side, ::std::vector<::std::unique_ptr<::LevelChunk>> levelChunks);

    


    virtual ~SchematicChunkSource() ;

    


    virtual bool canCreateViews() const ;

    





    virtual ::std::shared_ptr<::LevelChunk>
    getOrLoadChunk(::ChunkPos const& cp, ::ChunkSource::LoadMode lm, bool readOnly) ;

    



    virtual bool isChunkKnown(::ChunkPos const& chunkPos) ;

    





    virtual ::std::shared_ptr<::LevelChunk>
    createNewChunk(::ChunkPos const& cp, ::ChunkSource::LoadMode lm, bool readOnly) ;

    



    virtual ::std::shared_ptr<::LevelChunk> getExistingChunk(::ChunkPos const& cp) ;
};

} 
