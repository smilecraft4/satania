#version 460 core

layout(location = 0) out vec4 Frag_color;

in vec4 v_color;

uniform vec4 _Color;

void main()
{
    Frag_color = v_color;
}