#version 330 core

uniform vec3 light_dir; // representative of vlight, normalized from CPU

uniform sampler2D shadow_map;

/* Variables for lighting (all models) */
const vec3 light_color = vec3(1.0, 1.0, 1.0);
const vec3 surface_color = vec3(0.8, 0.8, 1.0);
uniform float kd;
uniform float ks;
/* Variables for Blinn-Phong lighting (isotropic and anisotropic) */
uniform float shininess;

smooth in vec3 vnormal;
smooth in vec3 vtangent;
smooth in vec3 vbitangent;
smooth in vec3 vview;
smooth in vec4 vshadowpos;

layout(location = 0) out vec4 fcolor;


/* Isotropic Blinn-Phong lighting */
vec3 blinn_phong(vec3 N, vec3 L, vec3 V, vec3 H)
{
    vec3 diffuse = kd * light_color * max(dot(L, N),0);
    vec3 specular = ks * light_color * pow(max(dot(H, N),0), shininess);
    return diffuse + specular;
}

float calculate_shadow(vec4 shadowPos, vec3 N, vec3 L)
{
    //  transform ortho coord to proj coord
    vec3 projCoord = shadowPos.xyz / shadowPos.w;

    //  transform to [0,1] range
    projCoord = projCoord * 0.5 + 0.5;

    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadow_map, projCoord.xy).r; 

    // get depth of current fragment from light's perspective
    float currentDepth = projCoord.z;

    float bias = max(0.05 * (1.0 - dot(L, N)), 0.005);

    // check whether current frag pos is in shadow
    // float shadow = (currentDepth - bias) > closestDepth  ? 1.0 : 0.0;

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_map, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadow_map, projCoord.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main(void)
{
    // Normalize the input from the vertex shader
    vec3 N = normalize(vnormal);
    vec3 T = normalize(vtangent);
    vec3 B = normalize(vbitangent);
    vec3 L = -light_dir;
    vec3 V = normalize(vview);
    vec3 H = normalize(L + V);

    //  lastly, compensate shadow by ambient light
    vec3 ambient = vec3(0.05);

    //  calculate shadow intensity
    float shadow = calculate_shadow(vshadowpos, N, L);

    // Evaluate the lighting model
    vec3 color = blinn_phong(N, L, V, H);

    // Resulting color at this fragment:
    fcolor = vec4(surface_color * ((1.0 - shadow) * color + ambient), 1.0);
}
