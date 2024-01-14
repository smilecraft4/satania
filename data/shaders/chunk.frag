#version 460 core

layout(location = 0) out vec4 FragColor;

in vec4 f_color;

void main()
{
    if (f_color.a < 0.5)
    {
       discard;
    }

    FragColor = f_color;
}