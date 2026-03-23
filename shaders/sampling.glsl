#ifndef SAMPLING_GLSL
#define SAMPLING_GLSL

#include "common.glsl"
#include "random.glsl"
#include "math.glsl"

struct LightSample {
    vec3 L;         
    float pdf;      
    vec3 emission;  
    float dist;     
};

// 【核心修复】：免疫高光 NaN 溢出的安全 MIS 权重算法
float powerHeuristic(float pdfA, float pdfB) {
    if (pdfA <= 0.0) return 0.0;
    if (pdfB <= 0.0) return 1.0;
    if (pdfA > pdfB) {
        float ratio = pdfB / pdfA;
        return 1.0 / (1.0 + ratio * ratio);
    } else {
        float ratio = pdfA / pdfB;
        return (ratio * ratio) / (1.0 + ratio * ratio);
    }
}

vec3 cosineSampleHemisphere(vec3 n, inout uint seed) {
    float r1 = rnd(seed); float r2 = rnd(seed);
    float z = sqrt(1.0 - r2); float phi = 2.0 * PI * r1;
    return buildTBN(n) * vec3(cos(phi) * sqrt(r2), sin(phi) * sqrt(r2), z);
}

vec3 importanceSampleGGX(vec2 xi, vec3 n, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return buildTBN(n) * vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

LightSample sampleSun(vec3 sunDir, float sunAngle, float sunIntensity, inout uint seed) {
    LightSample ls;
    float cosThetaMax = cos(sunAngle); 
    float solidAngle = 2.0 * PI * (1.0 - cosThetaMax);
    
    float r1 = rnd(seed); float r2 = rnd(seed);
    float cosTheta = (1.0 - r1) + r1 * cosThetaMax;
    float sinTheta = sqrt(max(0.0, 1.0 - cosTheta * cosTheta));
    float phi = 2.0 * PI * r2;
    
    vec3 d = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    
    ls.L = buildTBN(sunDir) * d;
    ls.dist = 10000.0; 
    ls.pdf = 1.0 / solidAngle;
    ls.emission = vec3(1.0) * sunIntensity / solidAngle; 
    return ls;
}

#endif