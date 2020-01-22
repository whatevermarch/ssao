const vec3 surface_color = vec3(0.8, 0.8, 1.0);

smooth in vec3 vposition;  // position in eye space
smooth in vec3 vnormal;    // normal in eye space, not normalized
smooth in vec4 vshadowpos;  // view vector in eye space, not normalized

layout(location = 0) out vec3 g_position;
layout(location = 1) out vec3 g_normal;
layout(location = 2) out vec3 g_albedo;
layout(location = 3) out vec3 g_shadow;

void main()
{    
    // store the fragment position vector in the first gbuffer texture
    g_position = vposition;

    // also store the per-fragment normals into the gbuffer
    g_normal = normalize(vnormal);

    // and the diffuse per-fragment color
    g_albedo.rgb = surface_color;

    //  don't forget to store clip-space vertex position in shadow pass
    g_shadow = vshadowpos.xyz / vshadowpos.w;
}