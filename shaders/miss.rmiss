#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(location = 0) rayPayloadInEXT RayPayload prd;
layout(push_constant) uniform PushConstantsBlock { PushConstants pc; };

void main() {
    vec3 dir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (dir.y + 1.0);
    
    vec3 skyBase = mix(vec3(1.0), pc.envColor_LgAng.xyz, t);
    float skyInt = pc.camRight_EnvInt.w;

    prd.emission = skyBase * skyInt;
    prd.throughputWeight = vec3(0.0);
    prd.depth = 999;
}