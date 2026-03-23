#version 460
#extension GL_EXT_ray_tracing : require

struct RayPayload {
    vec3 emission;
    vec3 albedo;
    uint depth;
    uint seed;
    vec3 rayOrigin;
    vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload prd;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
    vec4 cameraDir;
    vec4 cameraUp;
    vec4 cameraRight;
    vec4 projParams;
    vec4 envConfig; // xyz: color * intensity
    vec4 matParams1;
    vec4 matParams2;
} pc;

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    
    // 使用 PushConstants 传入的 UI 环境光配置，保留简单的天空盒梯度
    vec3 skyColor = mix(vec3(1.0), pc.envConfig.xyz, t);
    
    prd.emission = skyColor;
    prd.albedo = vec3(0.0);
    prd.depth = 999;
}