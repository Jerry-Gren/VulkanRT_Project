#ifndef SAMPLING_GLSL
#define SAMPLING_GLSL

#include "common.glsl"
#include "random.glsl"
#include "math.glsl"

vec3 cosineSampleHemisphere(vec3 n, inout uint seed) {
    float r1 = rnd(seed);
    float r2 = rnd(seed);
    float z = sqrt(1.0 - r2);
    float phi = 2.0 * PI * r1;
    return buildTBN(n) * vec3(cos(phi) * sqrt(r2), sin(phi) * sqrt(r2), z);
}

vec3 importanceSampleGGX(vec2 xi, vec3 n, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return buildTBN(n) * vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

#endif