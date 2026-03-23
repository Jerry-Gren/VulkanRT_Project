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
    float a = roughness * roughness;
    float a2 = max(a * a, 1e-6); 
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness) {
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
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
    float G = GeometrySmith(NdotV, NdotL, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 1e-4);
    
    // 【补偿】近似 Kulla-Conty 多次散射，补回因 G 项遮挡而丢失的能量
    vec3 msCompensation = F * (1.0 - G) * roughness / PI;
    specular += msCompensation * NdotL;

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
    float HdotV = max(dot(H, V), 0.0);

    float pdfDiffuse = NdotL / PI;
    float pdfSpecular = (D_GGX(NdotH, roughness) * NdotH) / max(4.0 * HdotV, 1e-4);

    float F = mix(0.04, 1.0, metallic);
    float eSpec = F;
    float eDiff = (1.0 - F) * (1.0 - metallic) * (1.0 - transmission);
    float sum = eSpec + eDiff;
    if(sum < 1e-4) return 0.0;
    
    return (eDiff / sum) * pdfDiffuse + (eSpec / sum) * pdfSpecular;
}

#endif