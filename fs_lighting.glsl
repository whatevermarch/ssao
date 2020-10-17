uniform sampler2D g_position;
uniform sampler2D g_normal;
uniform sampler2D g_albedo;
uniform sampler2D g_shadow;
uniform sampler2D ssao_texture;

uniform sampler2D shadow_map;

//  representative of vlight, normalized from CPU
uniform vec3 light_dir; 

//  Variables for lighting (all models)
const vec3 light_color = vec3(1.0, 1.0, 1.0);
uniform float kd;
uniform float ks;
//  Variables for Blinn-Phong lighting (isotropic and anisotropic)
uniform float shininess;

smooth in vec2 vtexcoord;

layout(location = 0) out vec4 fcolor;

//  Isotropic Blinn-Phong lighting
vec3 blinn_phong(vec3 N, vec3 L, vec3 V, vec3 H)
{
    vec3 diffuse = kd * light_color * max(dot(L, N), 0);
    vec3 specular = ks * light_color * pow(max(dot(H, N), 0), shininess);
    return diffuse + specular;
}

float calculate_shadow(vec3 S, vec3 N, vec3 L)
{
    //  transform ortho coord to proj coord
    vec3 projCoord = S;

    // get depth of current fragment from light's perspective
    float currentDepth = projCoord.z;

    float bias = max(0.05 * (1.0 - dot(L, N)), 0.005);

    // check whether current frag pos is in shadow using PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadow_map, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
            float pcfDepth = texture(shadow_map, projCoord.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
        }
    }
    shadow /= 9.0;

    return shadow;
}

vec3 calculate_ambient(float aofactor, vec3 albedo)
{
    return 0.3 * aofactor * albedo;
}

void main(void)
{
    //  Normalize the input from the vertex shader
    vec3 N = texture(g_normal, vtexcoord).rgb;
    vec3 L = -light_dir;
    vec3 V = normalize(-texture(g_position, vtexcoord).rgb); // vector towards the eye
    vec3 H = normalize(L + V);
    vec3 S = texture(g_shadow, vtexcoord).rgb;

    float aofactor = texture(ssao_texture, vtexcoord).r;

    vec3 albedo = texture(g_albedo, vtexcoord).rgb;

    //  lastly, compensate shadow by ambient light
    vec3 ambient = calculate_ambient(aofactor, albedo);

    //  calculate shadow intensity
    float shadow = calculate_shadow(S, N, L);

    //  Evaluate the lighting model
    vec3 color = blinn_phong(N, L, V, H);

    //  Resulting color at this fragment:
    fcolor = vec4(albedo * ((1.0 - shadow) * color + ambient), 1.0);
}