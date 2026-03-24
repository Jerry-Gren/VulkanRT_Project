#ifndef BSDF_GLSL
#define BSDF_GLSL

#include "common.glsl"

float fresnelDielectric(float cosThetaI, float etaI, float etaT) {
    cosThetaI = clamp(cosThetaI, -1.0, 1.0);
    if (cosThetaI < 0.0) { float temp = etaI; etaI = etaT; etaT = temp; cosThetaI = -cosThetaI; }
    float sinThetaI = sqrt(max(0.0, 1.0 - cosThetaI * cosThetaI));
    float sinThetaT = etaI / etaT * sinThetaI;
    if (sinThetaT >= 1.0) return 1.0; 
    float cosThetaT = sqrt(max(0.0, 1.0 - sinThetaT * sinThetaT));
    float Rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) / ((etaT * cosThetaI) + (etaI * cosThetaT));
    float Rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) / ((etaI * cosThetaI) + (etaT * cosThetaT));
    return (Rparl * Rparl + Rperp * Rperp) / 2.0;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float D_GGX(float NdotH, float roughness) {
    float a = max(roughness * roughness, 1e-5);
    float a2 = a * a;
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// 供直接光照 (NEE) 评估使用的精确 Smith 遮蔽函数
float Vis_Smith_Correlated(float NdotV, float NdotL, float roughness) {
    float a = max(roughness * roughness, 1e-5);
    float a2 = a * a;
    float ggxV = NdotL * sqrt(NdotV * NdotV * (1.0 - a2) + a2);
    float ggxL = NdotV * sqrt(NdotL * NdotL * (1.0 - a2) + a2);
    return 0.5 / max(ggxV + ggxL, 1e-5);
}

// 独立的 G1 函数（用于计算 PDF）
float G1_GGX(float NdotV, float roughness) {
    float a = max(roughness * roughness, 1e-5);
    float a2 = a * a;
    float NdotV2 = NdotV * NdotV;
    return (2.0 * NdotV) / (NdotV + sqrt(a2 + (1.0 - a2) * NdotV2));
}

// 【核心新增】：专为 VNDF 采样推导的权重化简项 G2 / G1，绝对保证能量守恒 (<= 1.0)
float G2_over_G1(float NdotV, float NdotL, float roughness) {
    float a = max(roughness * roughness, 1e-5);
    float a2 = a * a;
    float lambdaV = NdotL * sqrt(a2 + (1.0 - a2) * NdotV * NdotV);
    float lambdaL = NdotV * sqrt(a2 + (1.0 - a2) * NdotL * NdotL);
    return (NdotL * (NdotV + sqrt(a2 + (1.0 - a2) * NdotV * NdotV))) / max(lambdaV + lambdaL, 1e-5);
}

vec3 evalBRDF(vec3 V, vec3 L, vec3 N, vec3 albedo, float roughness, float metallic, float transmission) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    if (NdotL <= 0.0 || NdotV <= 0.0) return vec3(0.0);

    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    
    float D = D_GGX(NdotH, roughness);
    float Vis = Vis_Smith_Correlated(NdotV, NdotL, roughness);

    vec3 specular = D * Vis * F;
    
    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic) * (1.0 - transmission);
    vec3 diffuse = kD * albedo / PI;

    return diffuse + specular;
}

float pdfBRDF(vec3 V, vec3 L, vec3 N, float roughness, float metallic, float transmission) {
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    if (NdotL <= 0.0 || NdotV <= 0.0) return 0.0;

    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);

    float pdfDiffuse = NdotL / PI;
    
    float D = D_GGX(NdotH, roughness);
    float G1_V = G1_GGX(NdotV, roughness);
    float pdfSpecular = (D * G1_V) / max(4.0 * NdotV, 1e-5); 

    float F = mix(0.04, 1.0, metallic);
    float eSpec = F;
    float eDiff = (1.0 - F) * (1.0 - metallic) * (1.0 - transmission);
    float sum = eSpec + eDiff;
    if(sum < 1e-4) return 0.0;
    
    return (eDiff / sum) * pdfDiffuse + (eSpec / sum) * pdfSpecular;
}

#endif