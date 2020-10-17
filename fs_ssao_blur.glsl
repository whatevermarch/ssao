uniform sampler2D ssao_texture;

smooth in vec2 vtexcoord;

layout(location = 0) out float fcolor;

void main() 
{
    //  calculate texel size
    vec2 texelSize = 1.0 / vec2(textureSize(ssao_texture, 0));

    //  apply low-pass filter (blur)
    float result = 0.0;
    for (int x = -2; x < 2; x++) 
    {
        for (int y = -2; y < 2; y++) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssao_texture, vtexcoord + offset).r;
        }
    }
    fcolor = result / (4.0 * 4.0);
} 