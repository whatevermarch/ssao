#version 330 core

uniform mat4 projection_matrix;
uniform mat4 modelview_matrix;
uniform mat3 normal_matrix;

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;

smooth out vec3 vposition;  // position in eye space
smooth out vec3 vnormal;    // normal in eye space, not normalized

void main()
{
    //  calculate position in view space 
    vec3 pos = (modelview_matrix * position).xyz;
    vposition = pos;

    //  calculate normal in view space
    vnormal = normal_matrix * normal;
    
    //  calculate position in projection space
    gl_Position = projection_matrix * vec4(pos, 1.0);
}