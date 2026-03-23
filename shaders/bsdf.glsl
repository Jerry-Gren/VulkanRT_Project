#ifndef BSDF_GLSL
#define BSDF_GLSL

// 物理精确的电介质菲涅尔方程 (仅用于绝缘体)
float fresnelDielectric(float cosThetaI, float etaI, float etaT) {
    cosThetaI = clamp(cosThetaI, -1.0, 1.0);
    if (cosThetaI < 0.0) {
        float temp = etaI; etaI = etaT; etaT = temp;
        cosThetaI = -cosThetaI;
    }
    float sinThetaI = sqrt(max(0.0, 1.0 - cosThetaI * cosThetaI));
    float sinThetaT = etaI / etaT * sinThetaI;
    if (sinThetaT >= 1.0) return 1.0; 
    float cosThetaT = sqrt(max(0.0, 1.0 - sinThetaT * sinThetaT));
    float Rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) / ((etaT * cosThetaI) + (etaI * cosThetaT));
    float Rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) / ((etaI * cosThetaI) + (etaT * cosThetaT));
    return (Rparl * Rparl + Rperp * Rperp) / 2.0;
}

// TODO: 为下一步 MIS 预留的 BSDF 评估函数接口
// float evalGGX(vec3 V, vec3 L, vec3 N, float roughness) { ... }
// float pdfGGX(vec3 V, vec3 L, vec3 N, float roughness) { ... }

#endif