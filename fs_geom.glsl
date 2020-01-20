#version 330 core

smooth in vec3 vposition;  // position in eye space
smooth in vec3 vnormal;    // normal in eye space, not normalized

layout(location = 0) out vec3 g_position;
layout(location = 1) out vec3 g_normal;
layout(location = 2) out vec3 g_albedo;

void main()
{    
    // store the fragment position vector in the first gbuffer texture
    g_position = vposition;

    // also store the per-fragment normals into the gbuffer
    g_normal = normalize(vnormal);

    // and the diffuse per-fragment color
    g_albedo.rgb = vec3(0.95);
}