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

// 废弃旧的 importanceSampleGGX，改用 Heitz 2018 的 VNDF 采样
vec3 sampleGGXVNDF(vec3 V_world, vec3 N_world, float roughness, inout uint seed) {
    mat3 tbn = buildTBN(N_world);
    vec3 V = transpose(tbn) * V_world; // 转换到切线空间
    float alpha = max(roughness * roughness, 1e-5);
    
    vec3 Vh = normalize(vec3(alpha * V.x, alpha * V.y, V.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
    vec3 T2 = cross(Vh, T1);
    
    float r = sqrt(rnd(seed));
    float phi = 2.0 * PI * rnd(seed);
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    vec3 Ne = normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));
    
    return tbn * Ne; // 转回世界空间
}

LightSample sampleSun(vec3 sunDir, float sunAngle, float sunIntensity, inout uint seed) {
    LightSample ls;
    // 防止 sunAngle 为 0 导致 cosThetaMax = 1.0 从而引发除零 NaN
    float cosThetaMax = min(cos(sunAngle), 0.999999);
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