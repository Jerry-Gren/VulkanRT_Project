#ifndef COMMON_GLSL
#define COMMON_GLSL

const float PI = 3.14159265359;

struct Vertex { vec3 pos; vec3 normal; vec2 uv; };
struct SubMesh { uint firstIndex; uint indexCount; int vertexOffset; uint materialIndex; };
struct GPUMaterial { vec4 baseColor; float metallic; float roughness; float transmission; float ior; int textureID; int pad[3]; };

struct GPULight {
    vec4 position; 
    vec4 emission; 
    vec4 u;        
    vec4 v;        
};

struct RayPayload { 
    vec3 emission; 
    vec3 throughputWeight; 
    uint depth; 
    uint seed; 
    vec3 rayOrigin; 
    vec3 rayDir; 
};

struct PushConstants {
    vec4 camPos_Fov;       
    vec4 camDir_Aspect;    
    vec4 camUp_Frame;      
    vec4 camRight_EnvInt;  
    vec4 envColor_LgAng;   
    vec4 lightDir_LgInt;   
    vec4 albedo_Rough;     
    vec4 matParams;        
};

#endif