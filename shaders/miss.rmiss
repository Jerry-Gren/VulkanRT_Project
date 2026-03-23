#version 460
#extension GL_EXT_ray_tracing : require

struct RayPayload {
    vec3 emission;
    vec3 throughputWeight;
    uint depth;
    uint seed;
    vec3 rayOrigin;
    vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload prd;

// Miss Shader 也必须同步使用极致打包布局
layout(push_constant) uniform PushConstants {
    vec4 camPos_Fov;       
    vec4 camDir_Aspect;    
    vec4 camUp_Frame;      
    vec4 camRight_EnvInt;  // w: envIntensity
    vec4 envColor_LgAng;   // xyz: envColor
    vec4 lightDir_LgInt;   
    vec4 albedo_Rough;     
    vec4 matParams;        
} pc;

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    
    // 提取天空颜色和亮度
    vec3 skyBase = mix(vec3(1.0), pc.envColor_LgAng.xyz, t);
    float skyInt = pc.camRight_EnvInt.w;

    prd.emission = skyBase * skyInt;
    prd.throughputWeight = vec3(0.0);
    prd.depth = 999;
}