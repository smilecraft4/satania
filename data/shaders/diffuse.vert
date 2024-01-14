#version 460 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

uniform mat4 _Model;
uniform mat4 _View;
uniform mat4 _Projection;

out vec4 v_color;

void main()
{
    v_color = color;
    gl_Position = _Projection * _View * vec4(position.xyz, 1.0);
}