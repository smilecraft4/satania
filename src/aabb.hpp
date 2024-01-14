#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

struct AABB_mesh
{
    GLuint vao;
    GLuint vbo;
};

struct AABB
{
    glm::vec3 min;
    GLfloat p_min[1];
    glm::vec3 max;
    GLfloat p_max[1];
    glm::vec3 center;
    GLfloat p_center[1];
};