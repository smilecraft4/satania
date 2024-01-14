#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 uv;

out vec4 v_color;
//out vec4 f_color;

uniform mat4 _Projection;
uniform mat4 _View;
uniform float _Radius;

void main()
{
    v_color = color;
    gl_Position = vec4(position, 1.0);

    //f_color = color;
    //gl_Position = _Projection * _View * vec4(position, 1.0);
}