#version 460 core

layout(points) in;
layout(triangle_strip, max_vertices = 24) out;

in vec4 v_color[];

out vec4 f_color;

uniform mat4 _Projection;
uniform mat4 _View;
uniform float _Radius;

void main()
{
    vec4 pos = gl_in[0].gl_Position;

    f_color = v_color[0];

    if (f_color.a > 0.2)
    {

        gl_Position = _Projection * _View * (pos + vec4(-_Radius, -_Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(-_Radius, -_Radius, _Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(-_Radius, _Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(-_Radius, _Radius, _Radius, 0.0));
        EmitVertex();
        EndPrimitive();

        gl_Position = _Projection * _View * (pos + vec4(_Radius, -_Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, -_Radius, _Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, _Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, _Radius, _Radius, 0.0));
        EmitVertex();
        EndPrimitive();

        gl_Position = _Projection * _View * (pos + vec4(-_Radius, -_Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, -_Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(-_Radius, _Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, _Radius, -_Radius, 0.0));
        EmitVertex();
        EndPrimitive();

        gl_Position = _Projection * _View * (pos + vec4(-_Radius, -_Radius, _Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, -_Radius, _Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(-_Radius, _Radius, _Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, _Radius, _Radius, 0.0));
        EmitVertex();
        EndPrimitive();

        gl_Position = _Projection * _View * (pos + vec4(-_Radius, _Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, _Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(-_Radius, _Radius, _Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, _Radius, _Radius, 0.0));
        EmitVertex();
        EndPrimitive();

        gl_Position = _Projection * _View * (pos + vec4(-_Radius, -_Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, -_Radius, -_Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(-_Radius, -_Radius, _Radius, 0.0));
        EmitVertex();
        gl_Position = _Projection * _View * (pos + vec4(_Radius, -_Radius, _Radius, 0.0));
        EmitVertex();
        EndPrimitive();
    }
}