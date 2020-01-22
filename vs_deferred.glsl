layout (location = 0) in vec4 position;             
layout (location = 2) in vec2 texCoord;             
                                                                
smooth out vec2 vtexcoord;                          
                                                                
void main()                                         
{                                                   
    vtexcoord = texCoord;                           
    gl_Position = position;                         
}                                                   