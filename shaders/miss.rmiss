#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(location = 0) rayPayloadInEXT RayPayload prd;
layout(push_constant) uniform PushConstantsBlock { PushConstants pc; };

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    vec3 sunDir = normalize(pc.lightDir_LgInt.xyz);
    float sunAngle = pc.envColor_LgAng.w;
    // 防止除零
    float cosMax = min(cos(sunAngle), 0.999999);
    
    if (dot(dir, sunDir) >= cosMax) {
        float solidAngle = 2.0 * PI * (1.0 - cosMax);
        float sunIntensity = pc.lightDir_LgInt.w;
        // 精确返回 Radiance，不再除以 pdf
        prd.emission = vec3(1.0) * sunIntensity / solidAngle;
    } else {
        float t = 0.5 * (dir.y + 1.0);
        vec3 skyBase = mix(vec3(1.0), pc.envColor_LgAng.xyz, t);
        prd.emission = skyBase * pc.camRight_EnvInt.w;
    }
    
    prd.throughputWeight = vec3(0.0);
    prd.depth = 999;
}