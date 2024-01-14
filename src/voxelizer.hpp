#pragma once

#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

struct Chunk
{
    std::vector<glm::vec4> chunk;
};


/*

struct Parameters
{
    double      resolution;
    std::string mesh_filename;
    std::string voxel_filename;
    std::string texture_filename;
};

int main(int argc, char* argv[])

    PARSE ARGUMENTS

    WINDOW CREATION

    Mesh source_mesh = loadMesh(param.mesh_filename);
    BVH source_mesh_bvh = generateBVH(source_mesh);
    
    while()
    generateChunk()

    RENDER LOOP

    while()
    {
        CREATE BVH

        CREATE VOXELIZED CHUNKS
    }

    CONVERT VOXELS TO SAVE FORMAT

    CLEAN-UP
*/


class Voxelizer
{
public:
    /**
     * @brief Construct a new Voxelizer object
     * 
     */
    Voxelizer();

    /**
     * @brief 
     * 
     * @return true 
     * @return false 
     */
    bool voxelizeChunk(glm::vec3 chunk_min, glm::ivec3 chunk_size);

    /**
     * @brief Retrieve the voxelized chunk if it is finished or else return false
     * 
     * @param chunk 
     * @return return true if the chunk has been retrieved returns false if the chunk is not yet voxelized
     */
    bool retrieveChunk(Chunk& chunk);

private:

};

