#version 460 core

const vec3 vertex_position[24] = vec3[](
    
    vec3(0.5, -0.5, -0.5), vec3(0.5, -0.5, 0.5),
    vec3(-0.5, -0.5, -0.5), vec3(-0.5, -0.5, 0.5),
    vec3(-0.5, 0.5, -0.5), vec3(-0.5, 0.5, 0.5),
    vec3(0.5, 0.5, -0.5), vec3(0.5, 0.5, 0.5),

    vec3(-0.5, 0.5, -0.5), vec3(0.5, 0.5, -0.5),
    vec3(-0.5, -0.5, -0.5), vec3(0.5, -0.5, -0.5),
    vec3(-0.5, -0.5, 0.5), vec3(0.5, -0.5, 0.5),
    vec3(-0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5),
    
    vec3(0.5, -0.5, 0.5), vec3(0.5, 0.5, 0.5),
    vec3(-0.5, -0.5, 0.5), vec3(-0.5, 0.5, 0.5),
    vec3(-0.5, -0.5, -0.5), vec3(-0.5, 0.5, -0.5),
    vec3(0.5, -0.5, -0.5), vec3(0.5, 0.5, -0.5)
);

uniform mat4 _Model;
uniform mat4 _View;
uniform mat4 _Projection;

void main()
{
    gl_Position = _Projection * _View * _Model * vec4(vertex_position[gl_VertexID], 1.0);
}