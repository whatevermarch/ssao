uniform sampler2D g_position;
uniform sampler2D g_normal;
uniform sampler2D noise_texture;

uniform vec3 sampling_points[64];

// SSAO parameters
const int kernelSize = 64;
const float radius = 0.5;
const float bias = 0.025;

// tile noise texture over screen based on screen dimensions divided by noise size
uniform vec2 noiseScale; 

uniform mat4 projection_matrix;

smooth in vec2 vtexcoord;

layout(location = 0) out float fcolor;

void main()
{
    // get input for SSAO algorithm
    vec3 fragPos = texture(g_position, vtexcoord).xyz;
    vec3 normal = normalize(texture(g_normal, vtexcoord).rgb);
    vec3 randomVec = normalize(texture(noise_texture, vtexcoord * noiseScale).xyz);

    // create TBN change-of-basis matrix: from tangent-space to view-space
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    // iterate over the sample kernel and calculate occlusion factor
    float occlusion = 0.0;
    for(int i = 0; i < kernelSize; ++i)
    {
        // get sample position
        vec3 sample_pos = TBN * sampling_points[i]; // from tangent to view-space
        sample_pos = fragPos + sample_pos * radius; 
        
        // project sample position (to sample texture) (to get position on screen/texture)
        vec4 offset = vec4(sample_pos, 1.0);
        offset = projection_matrix * offset; // from view to clip-space
        offset.xyz /= offset.w; // perspective divide
        offset.xyz = offset.xyz * 0.5 + 0.5; // transform to range 0.0 - 1.0
        
        // get sample depth
        float sampleDepth = texture(g_position, offset.xy).z; // get depth value of kernel sample
        
        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
        occlusion += (sampleDepth >= sample_pos.z + bias ? 1.0 : 0.0) * rangeCheck;           
    }
    occlusion = 1.0 - (occlusion / kernelSize);
    
    fcolor = occlusion;
}