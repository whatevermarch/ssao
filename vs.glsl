#version 330 core

uniform mat4 projection_matrix;
uniform mat4 modelview_matrix;
uniform mat3 normal_matrix;
uniform mat4 mvp_matrix_shadow;

layout(location = 0) in vec4 position;
layout(location = 1) in vec3 normal;

smooth out vec3 vnormal;    // normal in eye space, not normalized
smooth out vec3 vtangent;   // normal in eye space, not normalized
smooth out vec3 vbitangent; // normal in eye space, not normalized
smooth out vec3 vview;      // view vector in eye space, not normalized
smooth out vec4 vshadowpos;  // view vector in eye space, not normalized

void main(void)
{
    /* Lighting computation, in eye space. */
    
    // The position in eye space.
    vec3 pos = (modelview_matrix * position).xyz;
    
    // The eye position in eye space is always (0,0,0).
    vec3 eye_pos = vec3(0.0);
    
    // The normal in eye space.
    vnormal = normal_matrix * normal;
    
    // The tangent in eye space.
    // Note: normally these would be precomputed and stored with the model.
    // This method here is just a hack. For example, it will fail if the normal is
    // approximately parallel to the fixed vector used here. -> fixed!
    vtangent = cross(vnormal, vec3(0.0, 1.0, 0.0));
    if( vtangent == vec3(0.0) )
        vtangent = vec3(1.0, 0.0, 0.0);
    
    // The bitangent in eye space.
    vbitangent = cross(vnormal, vtangent);
    
    // The view vector: from vertex position to eye position
    vview = eye_pos - pos;

    //  calculate position in shadow map
    vshadowpos = mvp_matrix_shadow * position;

    gl_Position = projection_matrix * modelview_matrix * position;
}
