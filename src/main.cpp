#pragma region INCLUDE

#include <cstdio>
#include <filesystem>
#include <format>
#include <fstream>
#include <stdarg.h>
#include <thread>
#include <vector>

#define GLFW_INCLUDE_NONE
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <GLFW/glfw3.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glad/glad.h>
#include <glm/glm.hpp>

#define DISPLAY_MESH 1
#define MULTI_DISPLAY_MESH 0
#define WRITE_SCHEM 0
#define WRITE_MCA 1
#define MULTIPLE_CHUNK 0
#define ADAPTIVE_CHUNK 0
#define DEBUG_INFO_OPENGL 0
#define SATANIA_MULTITHREADING 1

#include "bvh.hpp"
#include "camera.hpp"
#include "mca.hpp"
#include "mesh.hpp"
#include "nbt.hpp"
#include "timer.hpp"

#pragma endregion

#pragma region DATA TYPES

struct Voxel {
    glm::vec4 color;
};

struct DispatchIndirectCommand {
    GLuint num_groups_x;
    GLuint num_groups_y;
    GLuint num_groups_z;
};

struct Chunk {
    double resolution;
    glm::ivec3 size;
    glm::vec3 position;
};

#pragma endregion

#pragma region FUNCTION PROTOTYPE

GLuint compileShader(const std::string &shader_path, GLenum shader_type);
GLuint linkShader(std::initializer_list<GLuint> shaders);
void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const *message,
                      void const *user_param);

glm::mat4 aabbModel(glm::dvec3 aabb_min, glm::dvec3 aabb_max) {
    glm::vec3 aabb_mesh_size = glm::vec3(aabb_max - aabb_min);
    glm::vec3 aabb_mesh_center = glm::vec3((aabb_min + aabb_max) / 2.0);
    glm::mat4 model = glm::translate(glm::mat4(1.0), glm::vec3(aabb_mesh_center)) *
                      glm::scale(glm::mat4(1.0), glm::vec3(aabb_mesh_size));

    return model;
}

#pragma endregion

void createSchematic(const std::string &filename, const int *voxels, glm::ivec3 size) {
    std::vector<uint8_t> blocks(size.x * size.y * size.z, 0);
    std::vector<uint8_t> data(size.x * size.y * size.z, 0);

    for (size_t v_i = 0; v_i < size.x * size.y * size.z; v_i++) {
        int v_x = v_i % size.x;
        int v_y = (v_i / size.x) % size.y;
        int v_z = v_i / (size.x * size.y);

        // order from x y z -> y x z
        int b_i = v_x + v_z * size.x + v_y * size.x * size.z;

        blocks[b_i] = (uint8_t)voxels[v_i];
    }

    nbt::bytes schem;
    nbt::addCompoundTag(schem, "Schematic");
    nbt::addShortTag(schem, "Width", size.x);
    nbt::addShortTag(schem, "Height", size.y);
    nbt::addShortTag(schem, "Length", size.z);
    nbt::addStringTag(schem, "Materials", "Alpha");
    nbt::addByteArrayTag(schem, "Blocks", blocks);
    nbt::addByteArrayTag(schem, "Data", data);
    nbt::addListTag(schem, "Entities", nbt::TAG_Compound, 0);
    nbt::addListTag(schem, "TileEntities", nbt::TAG_Compound, 0);
    nbt::addEndTag(schem);

    nbt::writeBytes(filename, schem);
}

constexpr int max_height = 256;

struct Parameters {
    std::string mesh_filename;
    std::string voxel_filename;
    float voxel_resolution;
    int triangleBVH;
    int nodeDepthBVH;
    int max_x;
    int max_y;
    int max_z;
} static params;

int main(int argc, char **argv) {
    params.mesh_filename = "./data/meshes/dragon.obj";
    params.voxel_filename = "./data/region/dragons";
    params.voxel_resolution = 0.0025f;
    params.triangleBVH = 64;
    params.nodeDepthBVH = 32;

    params.max_x = 512;
    params.max_y = max_height;
    params.max_z = 512;

    if (argc > 1) {
        params.mesh_filename = argv[1];
    }
    if (argc > 2) {
        params.voxel_filename = argv[2];
    }
    if (argc > 3) {
        params.voxel_resolution = (float)atof(argv[3]);
    }
    if (argc > 5) {
        params.triangleBVH = atoi(argv[4]);
        params.nodeDepthBVH = atoi(argv[5]);
    }
    if (argc > 8) {
        params.max_x = atoi(argv[6]);
        params.max_y = atoi(argv[7]);
        params.max_z = atoi(argv[8]);
    }

    printf("OPTIONS\n");
    printf("\tmesh_filename: \"%s\"\n", params.mesh_filename.c_str());
    printf("\tvoxel_filename: \"%s\"\n", params.voxel_filename.c_str());
    printf("\tvoxel_resolution: %f\n", params.voxel_resolution);
    printf("\ttriangleBVH: %i\n", params.triangleBVH);
    printf("\tnodeDepthBVH: %i\n", params.nodeDepthBVH);
    printf("\tmaxChunkSize: (%i, %i, %i)\n", params.max_x, params.max_y, params.max_z);

    glm::ivec3 chunks_size_chunks(params.max_x, params.max_y, params.max_z); // Size of a chunk in voxel

    std::string voxelname =
        params.voxel_filename.substr(params.voxel_filename.find_last_of("/") + 1, params.voxel_filename.size());

    Timer timer;

#pragma region WINDOW

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow *window = glfwCreateWindow(800, 600, "Compute Shader", NULL, NULL);
    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);

#pragma endregion

#if DEBUG_INFO_OPENGL

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(message_callback, nullptr);

#endif

#pragma region SHADERS

    timer.start();
    GLuint voxel_shader_compute = compileShader("data/shaders/voxelizer.comp", GL_COMPUTE_SHADER);
    GLuint voxel_program = linkShader({voxel_shader_compute});

    GLuint chunk_shader_vert = compileShader("data/shaders/chunk.vert", GL_VERTEX_SHADER);
    GLuint chunk_shader_geom = compileShader("data/shaders/chunk.geom", GL_GEOMETRY_SHADER);
    GLuint chunk_shader_frag = compileShader("data/shaders/chunk.frag", GL_FRAGMENT_SHADER);
    GLuint chunk_program = linkShader({chunk_shader_vert, chunk_shader_geom, chunk_shader_frag});

    GLuint aabb_shader_vert = compileShader("data/shaders/aabb.vert", GL_VERTEX_SHADER);
    GLuint aabb_shader_frag = compileShader("data/shaders/aabb.frag", GL_FRAGMENT_SHADER);
    GLuint aabb_program = linkShader({aabb_shader_vert, aabb_shader_frag});

    GLuint diffuse_shader_vert = compileShader("data/shaders/diffuse.vert", GL_VERTEX_SHADER);
    GLuint diffuse_shader_frag = compileShader("data/shaders/diffuse.frag", GL_FRAGMENT_SHADER);
    GLuint diffuse_program = linkShader({diffuse_shader_vert, diffuse_shader_frag});
    timer.stop();
    printf("[TIMER] Shader Compilation: %.2f ms\n", timer.elapsed<std::chrono::nanoseconds>().count() / 1'000'000.0);

    // Store Uniform locations
    GLint voxel_program_Uniform_AABB_min = glGetUniformLocation(voxel_program, "_AABB_min");
    GLint voxel_program_Uniform_AABB_max = glGetUniformLocation(voxel_program, "_AABB_max");
    GLint voxel_program_Uniform_Resolution = glGetUniformLocation(voxel_program, "_Resolution");
    GLint voxel_program_Uniform_ElementsCount = glGetUniformLocation(voxel_program, "_ElementsCount");
    GLint voxel_program_Uniform_TriangleCount = glGetUniformLocation(voxel_program, "_TriangleCount");
    GLint voxel_program_Uniform_ChunkSize = glGetUniformLocation(voxel_program, "_ChunkSize");

    GLint chunk_program_Uniform_Radius = glGetUniformLocation(chunk_program, "_Radius");
    GLint chunk_program_Uniform_View = glGetUniformLocation(chunk_program, "_View");
    GLint chunk_program_Uniform_Proj = glGetUniformLocation(chunk_program, "_Projection");

    GLint diffuse_program_Uniform_Color = glGetUniformLocation(diffuse_program, "_Color");
    GLint diffuse_program_Uniform_Model = glGetUniformLocation(diffuse_program, "_Model");
    GLint diffuse_program_Uniform_View = glGetUniformLocation(diffuse_program, "_View");
    GLint diffuse_program_Uniform_Proj = glGetUniformLocation(diffuse_program, "_Projection");

    GLint aabb_program_Uniform_Color = glGetUniformLocation(aabb_program, "_Color");
    GLint aabb_program_Uniform_Model = glGetUniformLocation(aabb_program, "_Model");
    GLint aabb_program_Uniform_View = glGetUniformLocation(aabb_program, "_View");
    GLint aabb_program_Uniform_Proj = glGetUniformLocation(aabb_program, "_Projection");

#pragma endregion

#pragma region LOADING MESH

    timer.start();
    static Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(params.mesh_filename, aiProcess_DropNormals | aiProcess_Triangulate);

    Mesh mesh;
    mesh.vertices.resize(scene->mMeshes[0]->mNumVertices);
    mesh.elements.resize(scene->mMeshes[0]->mNumFaces * 3);

    srand(time(0));
    for (unsigned int i = 0; i < scene->mMeshes[0]->mNumVertices; i++) {
        mesh.vertices[i].position.x = scene->mMeshes[0]->mVertices[i].x;
        mesh.vertices[i].position.y = scene->mMeshes[0]->mVertices[i].y;
        mesh.vertices[i].position.z = scene->mMeshes[0]->mVertices[i].z;
    }

    for (unsigned int i = 0; i < scene->mMeshes[0]->mNumFaces; i++) {
        mesh.elements[i * 3 + 0] = scene->mMeshes[0]->mFaces[i].mIndices[0];
        mesh.elements[i * 3 + 1] = scene->mMeshes[0]->mFaces[i].mIndices[1];
        mesh.elements[i * 3 + 2] = scene->mMeshes[0]->mFaces[i].mIndices[2];

        glm::vec4 color(1.0f);
        color.r = (rand() % 32) / 32.0;
        color.b = (rand() % 32) / 32.0;
        color.g = (rand() % 32) / 32.0;

        mesh.vertices[mesh.elements[i * 3 + 0]].color = color;
        mesh.vertices[mesh.elements[i * 3 + 1]].color = color;
        mesh.vertices[mesh.elements[i * 3 + 2]].color = color;
    }

    GLuint mesh_vao;
    GLuint mesh_vbo;
    GLuint mesh_ebo;

    glCreateVertexArrays(1, &mesh_vao);
    glCreateBuffers(1, &mesh_vbo);
    glCreateBuffers(1, &mesh_ebo);

    glBindVertexArray(mesh_vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh_vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * sizeof(mesh.vertices[0]), mesh.vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.elements.size() * sizeof(mesh.elements[0]), mesh.elements.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));

    timer.stop();
    printf("[TIMER] Mesh loading: %.2f ms\n", timer.elapsed<std::chrono::nanoseconds>().count() / 1'000'000.0);

#pragma endregion

#pragma region BVH

    GLuint aabb_mesh_vao;
    glCreateVertexArrays(1, &aabb_mesh_vao);
    std::vector<glm::mat4> aabb_mesh_models;
    std::vector<glm::vec4> aabb_mesh_colors;
    std::vector<glm::mat4> aabb_mesh_models_chunks;
    std::vector<glm::vec4> aabb_mesh_colors_chunks;

    timer.start(); // times the building of the BVH

    BVH mesh_bvh(mesh, params.triangleBVH, params.nodeDepthBVH);

    timer.stop();
    printf("[TIMER] BVH building: %.2f ms\n", timer.elapsed<std::chrono::nanoseconds>().count() / 1'000'000.0);

    printf("Node count: %zi\n", mesh_bvh.m_nodes.size());

    GLuint bvh_nodes;
    GLuint bvh_triangles;

    glCreateBuffers(1, &bvh_nodes);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvh_nodes);
    glBufferData(GL_SHADER_STORAGE_BUFFER, mesh_bvh.m_nodes.size() * sizeof(BVH::Node), mesh_bvh.m_nodes.data(),
                 GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, bvh_nodes);

    glCreateBuffers(1, &bvh_triangles);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bvh_triangles);
    glBufferData(GL_SHADER_STORAGE_BUFFER, mesh_bvh.m_triangles.size() * sizeof(BVH::Triangle),
                 mesh_bvh.m_triangles.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bvh_triangles);

    GLuint mesh_bvh_vao;
    GLuint mesh_bvh_vbo;

    glCreateVertexArrays(1, &mesh_bvh_vao);
    glCreateBuffers(1, &mesh_bvh_vbo);

    glBindVertexArray(mesh_bvh_vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh_bvh_vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh_bvh.m_triangles.size() * sizeof(BVH::Triangle), mesh_bvh.m_triangles.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));

    for (const auto &node : mesh_bvh.m_nodes) {
        // this is a node
        if (node.leaf_elem == 0) {
            aabb_mesh_models.push_back(aabbModel(node.aabb.min, node.aabb.max));
            aabb_mesh_colors.push_back(glm::vec4(0.0f, 0.0, 1.0, 1.0));
        }
        // this is a leaf
        else if (node.leaf_elem != 0) {
            aabb_mesh_models.push_back(aabbModel(node.aabb.min, node.aabb.max));
            aabb_mesh_colors.push_back(glm::vec4(0.0f, 1.0, 0.0, 1.0));
        }
    }

#pragma endregion

#pragma region CHUNKS
    int chunk_index = 0;

    auto a = glm::ivec3((mesh_bvh.m_nodes[0].aabb.max - mesh_bvh.m_nodes[0].aabb.min) / params.voxel_resolution);
    auto b = glm::ceilMultiple(a + 1, glm::ivec3(16));

#if WRITE_MCA
    glm::ivec3 chunks_voxels_size = chunks_size_chunks; // Size of a chunk in voxel
    chunks_voxels_size.y = b.y;
    chunks_size_chunks.y = b.y;

    if (chunks_voxels_size.y > max_height) {
        printf("Mesh bounding box is above the big maximum %i build height !!!\n", max_height);

        char ans = 'N';
        do {
            std::cout << "Do you want to continue (y/n)? or floor the bounding box to " << max_height << " (f) ? \n";
            std::cout << "You must type a 'Y' or an 'N' or an 'F':";
            std::cin >> ans;

            if ((ans == 'N') || (ans == 'n')) {
                exit(0);
            }

            if ((ans == 'F') || (ans == 'f')) {
                chunks_voxels_size.y = max_height;
                chunks_size_chunks.y = max_height;

                printf("Fllored it\n");
            }

        } while ((ans != 'Y') && (ans != 'y') && (ans != 'F') && (ans != 'f'));
    }
#else
#if ADAPTIVE_CHUNK

    glm::ivec3 chunks_voxels_size = glm::min(b, chunks_size_chunks); // Size of a chunk in voxel

#else
#if MULTIPLE_CHUNK
    glm::ivec3 chunks_voxels_size = chunks_size_chunks; // Size of a chunk in voxel
#else
    glm::ivec3 chunks_voxels_size = b; // Size of a chunk in voxel
#endif
#endif
#endif

    glm::ivec3 chunks_count = 1 + (glm::ivec3)(glm::vec3(mesh_bvh.m_nodes[0].aabb.max - mesh_bvh.m_nodes[0].aabb.min) /
                                               params.voxel_resolution) /
                                      chunks_voxels_size;

    printf("BOUNDING BOX MINIMUM SIZE: (%i, %i, %i)\n", b.x, b.y, b.z);
    printf("CHUNK SIZE: (%i, %i, %i)\n", chunks_voxels_size.x, chunks_voxels_size.y, chunks_voxels_size.z);
    printf("TOTAL SIZE: (%i, %i, %i)\n", chunks_voxels_size.x * chunks_count.x, chunks_voxels_size.y * chunks_count.y,
           chunks_voxels_size.z * chunks_count.z);

    glm::vec3 chunk_aabb_min;
    glm::vec3 chunk_aabb_max;
    GLsync chunk_sync; // Used to check if the compute shader as finished working

    // Chunk Arrays
    std::vector<std::vector<Voxel>> chunks_voxels;
    std::vector<GLuint> chunks_vaos;
    std::vector<GLuint> chunks_vbos;
    std::vector<GLuint> chunks_counts;

#pragma endregion

#pragma region VOXELIZER COMPUTE

    GLint work_group_size[3];
    glGetProgramiv(voxel_program, GL_COMPUTE_WORK_GROUP_SIZE, work_group_size);

    DispatchIndirectCommand compute_indirect_command_data{};
    compute_indirect_command_data.num_groups_x = (GLuint)std::ceil(chunks_voxels_size.x / work_group_size[0]);
    compute_indirect_command_data.num_groups_y = (GLuint)std::ceil(chunks_voxels_size.y / work_group_size[1]);
    compute_indirect_command_data.num_groups_z = (GLuint)std::ceil(chunks_voxels_size.z / work_group_size[2]);

    GLuint compute_indirect_command;
    glCreateBuffers(1, &compute_indirect_command);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, compute_indirect_command);
    glBufferData(GL_DISPATCH_INDIRECT_BUFFER, sizeof(compute_indirect_command_data), &compute_indirect_command_data,
                 GL_STATIC_DRAW);
    glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);

    // Persistant mapped buffer
    GLuint voxels_ssbo;
    GLbitfield voxels_ssbo_flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
    glCreateBuffers(1, &voxels_ssbo);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, voxels_ssbo);
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    chunks_voxels_size.x * chunks_voxels_size.y * chunks_voxels_size.z * sizeof(int), 0,
                    voxels_ssbo_flags);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, voxels_ssbo);
    int *voxel_ssbo_data = (int *)glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER, 0, chunks_voxels_size.x * chunks_voxels_size.y * chunks_voxels_size.z * sizeof(int),
        voxels_ssbo_flags);

    bool voxel_compute_finished = false;
    bool voxel_compute_started = false;

    bool voxel_compute_paused = false;

    bool voxel_compute_paused_pressed = false;

#pragma endregion

#pragma region RENDER LOOP

    int width, height;
    glm::dvec2 mouse_last{}, mouse_current{};
    glfwGetCursorPos(window, &mouse_last.x, &mouse_last.y);
    glfwGetCursorPos(window, &mouse_current.x, &mouse_current.y);

    Camera camera;

    static glm::ivec3 last_chunk_index_pos;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    std::string voxel_path = params.voxel_filename + "_commands.txt";

    FILE *commandFile = fopen(voxel_path.c_str(), "w");

#if WRITE_MCA
    // create the regions directory
    std::string mca_folder = params.voxel_filename;
    if (!std::filesystem::is_directory(mca_folder) && !std::filesystem::exists(mca_folder)) {
        // TODO check for errors
        std::filesystem::create_directory(mca_folder);
    }

#endif

    Timer total_voxelization_timer;
    // Update loop
    while (!glfwWindowShouldClose(window)) {
        // EVENTS PROCESSING

        glfwPollEvents();
        glfwGetWindowSize(window, &width, &height);

        mouse_last = mouse_current;
        glfwGetCursorPos(window, &mouse_current.x, &mouse_current.y);
        camera.update(window, glm::vec2(mouse_current - mouse_last));

        glViewport(0, 0, width, height);

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !voxel_compute_paused_pressed) {
            voxel_compute_paused = !voxel_compute_paused;
            voxel_compute_paused_pressed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE && voxel_compute_paused_pressed) {
            voxel_compute_paused_pressed = false;
        }

#pragma region DATA PROCESSING

        if (!voxel_compute_started && !voxel_compute_paused) {
            if (chunk_index == 0) {
                total_voxelization_timer.start();
            }
            // Check if the next chunks need to be computed
            if (chunk_index < chunks_count.x * chunks_count.y * chunks_count.z) {
                timer.start();

                // Get the chunk to be voxelized location
                glm::ivec3 next_chunk_pos;
                next_chunk_pos.x = chunk_index % chunks_count.x;
                next_chunk_pos.y = (chunk_index / chunks_count.x) % chunks_count.y;
                next_chunk_pos.z = chunk_index / (chunks_count.x * chunks_count.y);

                // Get the working area of the voxelizer
                chunk_aabb_min = mesh_bvh.m_nodes[0].aabb.min +
                                 glm::vec3(chunks_voxels_size) * params.voxel_resolution * glm::vec3(next_chunk_pos);
                chunk_aabb_max = mesh_bvh.m_nodes[0].aabb.max + glm::vec3(chunks_voxels_size) *
                                                                    params.voxel_resolution *
                                                                    glm::vec3(next_chunk_pos + 1);

                // Set all the uniforms for the compute program for the chunk to voxelize
                glUseProgram(voxel_program);
                glUniform1i(voxel_program_Uniform_ElementsCount, (GLint)(mesh_bvh.m_triangles.size()));
                glUniform1i(voxel_program_Uniform_TriangleCount, (GLint)(mesh_bvh.m_triangles.size()));
                glUniform3i(voxel_program_Uniform_ChunkSize, chunks_voxels_size.x, chunks_voxels_size.y,
                            chunks_voxels_size.z);
                glUniform1d(voxel_program_Uniform_Resolution, params.voxel_resolution);
                glUniform3d(voxel_program_Uniform_AABB_min, chunk_aabb_min.x, chunk_aabb_min.y, chunk_aabb_min.z);
                glUniform3d(voxel_program_Uniform_AABB_max, chunk_aabb_max.x, chunk_aabb_max.y, chunk_aabb_max.z);

                // Call compute shader to voxelize the chunk
                glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, compute_indirect_command);
                glDispatchComputeIndirect(0);
                glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

                chunk_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

                glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, 0);
                glUseProgram(0);

                voxel_compute_finished = false;
                voxel_compute_started = true;
            }
        }

        // Only called is currently computing
        if (!voxel_compute_finished && voxel_compute_started) {
            // Check if the chunk as finished processing and writting the voxel data
            GLenum compute_finished = glClientWaitSync(chunk_sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
            if ((compute_finished == GL_ALREADY_SIGNALED) || (compute_finished == GL_CONDITION_SATISFIED)) {
                voxel_compute_finished = true;
                timer.stop();
                printf("[TIMER] chunk %i/%i took: %.2f ms\n", chunk_index + 1,
                       chunks_count.x * chunks_count.y * chunks_count.z,
                       timer.elapsed<std::chrono::nanoseconds>().count() / 1'000'000.0);
                glDeleteSync(chunk_sync);

                if (chunk_index + 1 == chunks_count.x * chunks_count.y * chunks_count.z) {
                    total_voxelization_timer.stop();
                    printf("[TIMER] Voxelization took: %.2f ms\n",
                           total_voxelization_timer.elapsed<std::chrono::nanoseconds>().count() / 1'000'000.0);
                }
            }
        }

        // Only called if a chunk as finished working and a chunk is wating to be copied from the GPU
        if (voxel_compute_finished && voxel_compute_started) {
#if !MULTI_DISPLAY_MESH

            if (chunks_vaos.size() > 0) {
                glDeleteBuffers(chunks_vbos.size(), chunks_vbos.data());
                glDeleteVertexArrays(chunks_vaos.size(), chunks_vaos.data());
            }

            chunks_vbos.clear();
            chunks_vaos.clear();
            chunks_counts.clear();

#endif // !MULTI_DISPLAY_MESH
       // Create the mesh data to draw the chunk
#if DISPLAY_MESH
            std::vector<Vertex> chunk_vertices;
            for (int voxel_index = 0; voxel_index < chunks_voxels_size.x * chunks_voxels_size.y * chunks_voxels_size.z;
                 voxel_index++) {
                if (voxel_ssbo_data[voxel_index]) {
                    Vertex vertex;

                    glm::ivec3 chunk_voxel_position{};
                    chunk_voxel_position.x = voxel_index % chunks_voxels_size.x;
                    chunk_voxel_position.y = (voxel_index / chunks_voxels_size.x) % chunks_voxels_size.y;
                    chunk_voxel_position.z = voxel_index / (chunks_voxels_size.x * chunks_voxels_size.y);

                    vertex.color =
                        glm::vec4(glm::vec3(chunk_voxel_position) / glm::vec3(chunks_count * chunks_voxels_size), 1.0f);
                    vertex.position = chunk_aabb_min + glm::vec3(chunk_voxel_position) * params.voxel_resolution;

                    chunk_vertices.push_back(vertex);
                }
            }

            GLuint chunk_vao, chunk_vbo, chunk_count;
            glCreateVertexArrays(1, &chunk_vao);
            glCreateBuffers(1, &chunk_vbo);

            glBindVertexArray(chunk_vao);

            chunk_count = chunk_vertices.size();

            glBindBuffer(GL_ARRAY_BUFFER, chunk_vbo);
            glBufferData(GL_ARRAY_BUFFER, chunk_vertices.size() * sizeof(chunk_vertices[0]), chunk_vertices.data(),
                         GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)offsetof(Vertex, position));
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)offsetof(Vertex, color));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid *)offsetof(Vertex, uv));
            glEnableVertexAttribArray(2);

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindVertexArray(0);

            chunks_vaos.push_back(chunk_vao);
            chunks_vbos.push_back(chunk_vbo);
            chunks_counts.push_back(chunk_count);

#endif // MESH_DRAW

            glm::ivec3 chunk_index_pos{};
            chunk_index_pos.x = chunk_index % chunks_count.x;
            chunk_index_pos.y = (chunk_index / chunks_count.x) % chunks_count.y;
            chunk_index_pos.z = chunk_index / (chunks_count.x * chunks_count.y);

            glm::vec3 aabb_min = chunk_aabb_min + glm::vec3(chunk_index_pos) * params.voxel_resolution;
            glm::vec3 aabb_max =
                chunk_aabb_min + glm::vec3(chunk_index_pos + chunks_voxels_size) * params.voxel_resolution;

            aabb_mesh_models_chunks.push_back(aabbModel(aabb_min, aabb_max));
            aabb_mesh_colors_chunks.push_back(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

#if WRITE_SCHEM

            std::string export_path = "C:/Users/50nua/Documents/MultiMC/instances/Simply Optimized "
                                      "1.18.x-11.1/.minecraft/config/worldedit/schematics/";
            std::string schematic_name = "suzanne_small";

            createSchematic(params.voxel_filename + "_" + std::to_string(chunk_index_pos.x) + "_" +
                                std::to_string(chunk_index_pos.y) + "_" + std::to_string(chunk_index_pos.z) +
                                ".schematic",
                            voxel_ssbo_data, chunks_voxels_size);

            if (chunk_index > 0) {
                auto local_chunk_index_pos = chunk_index_pos - last_chunk_index_pos;
                std::string tp_command = std::format(
                    "/tp @p ~{} ~{} ~{}\n", local_chunk_index_pos.x * chunks_size_chunks.x,
                    local_chunk_index_pos.y * chunks_size_chunks.y, local_chunk_index_pos.z * chunks_size_chunks.z);

                fwrite(tp_command.c_str(), tp_command.size(), 1, commandFile);
            }

            std::string filename = voxelname + "_" + std::to_string(chunk_index_pos.x) + "_" +
                                   std::to_string(chunk_index_pos.y) + "_" + std::to_string(chunk_index_pos.z);

            std::string load_command = std::format("//schem load {}\n", filename);
            std::string paste_command = "//paste\n";

            fwrite(load_command.c_str(), load_command.size(), 1, commandFile);
            fwrite(paste_command.c_str(), paste_command.size(), 1, commandFile);

            last_chunk_index_pos = chunk_index_pos;

#endif // WRITE_SCHEM

#if WRITE_MCA

            std::string mca_file_name = mca_folder + "/r." + std::to_string(chunk_index_pos.x) + "." +
                                        std::to_string(chunk_index_pos.z) + ".mca";

            // prepare buffer data
            std::vector<uint64_t> voxel_data((chunks_voxels_size.x / 16) * chunks_voxels_size.x * chunks_voxels_size.y,
                                             0);

            Timer timerb;
            timerb.start();

#if SATANIA_MULTITHREADING
            int num_thread = std::thread::hardware_concurrency();
            int count = voxel_data.size() / num_thread;
            std::vector<std::thread> threads;

            for (int i = 0; i < 8; i++) {
                voxel_ssbo_data[i] = 1;
            }

            voxel_data[0] = 1;

            auto thread_function = [&voxel_data](int start, int count, const int *data) {
                for (int i = 0; i < voxel_data.size(); i++) {
                    for (int j = 0; j < 16; j++) {
                        uint64_t mask = static_cast<uint64_t>(1) << (j * 4);
                        int index = i * 16;

                        voxel_data[i] |= (static_cast<uint64_t>(data[index + j]) << (j * 4)) & mask;
                    }
                }
            };

            for (size_t t = 0; t < num_thread; t++) {
                int start = t * count;
                threads.push_back(std::thread(thread_function, start, count, voxel_ssbo_data));
            }
            // Wait for all the tread to be finished
            for (auto &thread : threads) {
                thread.join();
            }
#else
            uint64_t blocks{0};
            for (int i = 0; i < voxel_data.size(); i++) {
                for (int j = 0; j < 16; j++) {
                    uint64_t mask = static_cast<uint64_t>(1) << (j * 4);
                    int index = i * 16;

                    voxel_data[i] |= (static_cast<uint64_t>(voxel_ssbo_data[index + j]) << (j * 4)) & mask;
                }

                // if (voxel_data[i] != 0) {
                // 	printf("gegeg\n");
                // }

                blocks += voxel_data[i];
            }
#endif
            timerb.stop();
            printf("[TIMER] Voxel data sorting took: %.2f ms\n",
                   timerb.elapsed<std::chrono::nanoseconds>().count() / 1'000'000.0);
            // printf("WRITTEN: %llu blocks", blocks);

            timerb.start();
            mca::writeMCA(mca_file_name, chunk_index_pos.x, chunk_index_pos.z, {"minecraft:air", "minecraft:stone"},
                          voxel_data);
            timerb.stop();
            printf(
                "[TIMER] MCA writing of %s: %.2f ms\n",
                std::string("r." + std::to_string(chunk_index_pos.x) + "." + std::to_string(chunk_index_pos.z) + ".mca")
                    .c_str(),
                timerb.elapsed<std::chrono::nanoseconds>().count() / 1'000'000.0);

            // convert the data to mca data

            // create a region based on the chunk data and chunk location (of satania 512x256+x512)

#endif

            chunk_index++;
            voxel_compute_started = false;

            if (chunk_index >= chunks_count.x * chunks_count.y * chunks_count.z) {
                fclose(commandFile);
                printf("Minecraft World edit commands saved to \"%s\"\n", voxel_path.c_str());
                glUnmapBuffer(voxels_ssbo);
            }
        }

#pragma endregion

#pragma region OPENGL RENDERING

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(aabb_program);
        glUniformMatrix4fv(aabb_program_Uniform_View, 1, GL_FALSE, &camera._view[0][0]);
        glUniformMatrix4fv(aabb_program_Uniform_Proj, 1, GL_FALSE, &camera._projection[0][0]);
        glBindVertexArray(aabb_mesh_vao);
        glLineWidth(1.0f);
        for (size_t i = 0; i < aabb_mesh_models_chunks.size(); i++) {
            glUniformMatrix4fv(aabb_program_Uniform_Model, 1, GL_FALSE, &aabb_mesh_models_chunks[i][0][0]);
            glUniform4f(aabb_program_Uniform_Color, 1.0f, 0.0f, 0.0f, 1.0f);
            glDrawArrays(GL_LINES, 0, 24);
        }
        glBindVertexArray(0);
        glUseProgram(0);

        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            glUseProgram(diffuse_program);
            glUniformMatrix4fv(diffuse_program_Uniform_View, 1, GL_FALSE, &camera._view[0][0]);
            glUniformMatrix4fv(diffuse_program_Uniform_Proj, 1, GL_FALSE, &camera._projection[0][0]);
            glUniform4f(diffuse_program_Uniform_Color, 1.0f, 1.0f, 1.0f, 1.0f);
            glBindVertexArray(mesh_bvh_vao);
            glDrawArrays(GL_TRIANGLES, 0, mesh_bvh.m_triangles.size() * 3);
        } else if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) { // Draw the mesh bounding box
            glUseProgram(aabb_program);
            glUniformMatrix4fv(aabb_program_Uniform_View, 1, GL_FALSE, &camera._view[0][0]);
            glUniformMatrix4fv(aabb_program_Uniform_Proj, 1, GL_FALSE, &camera._projection[0][0]);
            glBindVertexArray(aabb_mesh_vao);
            glLineWidth(1.0f);
            for (size_t i = 0; i < aabb_mesh_models.size(); i++) {
                glUniformMatrix4fv(aabb_program_Uniform_Model, 1, GL_FALSE, &aabb_mesh_models[i][0][0]);
                glUniform4f(aabb_program_Uniform_Color, aabb_mesh_colors[i].r, aabb_mesh_colors[i].g,
                            aabb_mesh_colors[i].b, aabb_mesh_colors[i].a);
                glDrawArrays(GL_LINES, 0, 24);
            }
            glBindVertexArray(0);
            glUseProgram(0);
        } else { // Render all the computed chunk
            glUseProgram(chunk_program);
            glUniformMatrix4fv(chunk_program_Uniform_View, 1, GL_FALSE, &camera._view[0][0]);
            glUniformMatrix4fv(chunk_program_Uniform_Proj, 1, GL_FALSE, &camera._projection[0][0]);
            glUniform1f(chunk_program_Uniform_Radius, (GLfloat)(params.voxel_resolution / 2.0f));

            for (int i = 0; i < chunks_vaos.size(); i++) {
                glBindVertexArray(chunks_vaos[i]);
                glDrawArrays(GL_POINTS, 0, chunks_counts[i]);
                glBindVertexArray(0);
            }
        }

        glfwSwapBuffers(window);

#pragma endregion
    }

#pragma endregion

#pragma region CLEAN UP

    for (int i = 0; i < chunks_voxels.size(); i++) {
        // free(chunks_voxels[i]);
        // chunks_voxels[i] = NULL;

        glDeleteBuffers(1, &chunks_vbos[i]);
        glDeleteVertexArrays(1, &chunks_vaos[i]);
    }

    glDeleteBuffers(1, &mesh_ebo);
    glDeleteBuffers(1, &mesh_vbo);
    glDeleteProgram(voxel_program);
    glDeleteProgram(chunk_program);

    glfwDestroyWindow(window);
    glfwTerminate();

#pragma endregion

    return 0;
}

#pragma region FUNCTION DEFINITION

GLuint compileShader(const std::string &shader_path, GLenum shader_type) {
    std::ifstream shader_file(shader_path);
    std::string shader_code;
    std::string line;

    while (std::getline(shader_file, line)) {
        shader_code.append(line + "\n");
    }

    const char *shader_source = shader_code.c_str();

    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &shader_source, NULL);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char buf[4096];
        glGetShaderInfoLog(shader, 4096, 0, buf);
        printf("%s\n", buf);
    }

    return shader;
}

GLuint linkShader(std::initializer_list<GLuint> shaders) {
    GLuint program = glCreateProgram();
    for (auto shader : shaders) {
        glAttachShader(program, shader);
    }
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char buf[4096];
        glGetProgramInfoLog(program, 4096, 0, buf);
        printf("%s\n", buf);
    }
    return program;
}

void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const *message,
                      void const *user_param) {
    auto const src_str = [source]() {
        switch (source) {
        case GL_DEBUG_SOURCE_API:
            return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            return "WINDOW SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            return "SHADER COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            return "THIRD PARTY";
        case GL_DEBUG_SOURCE_APPLICATION:
            return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER:
            return "OTHER";
        default:
            return "UNKNOWN";
        }
    }();

    auto const type_str = [type]() {
        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            return "DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY:
            return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE:
            return "PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER:
            return "MARKER";
        case GL_DEBUG_TYPE_OTHER:
            return "OTHER";
        default:
            return "UNKNOWN";
        }
    }();

    auto const severity_str = [severity]() {
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            return "note";
        case GL_DEBUG_SEVERITY_LOW:
            return "low";
        case GL_DEBUG_SEVERITY_MEDIUM:
            return "Medium";
        case GL_DEBUG_SEVERITY_HIGH:
            return "HIGH";
        default:
            return "UNKNOWN";
        }
    }();

    printf("[%s]: %s %s (%i): %s\n", src_str, type_str, severity_str, id, message);
}

#pragma endregion