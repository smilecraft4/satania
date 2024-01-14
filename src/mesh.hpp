#pragma once

struct Vertex
{
    glm::vec3 position;
    GLfloat p_pos[1];
    glm::vec4 color;
    glm::vec2 uv;
    GLfloat p_uv[2];
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<unsigned int> elements;
};
