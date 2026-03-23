#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"
#include "random.glsl"
#include "math.glsl"
#include "sampling.glsl"
#include "bsdf.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0, scalar) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 3, set = 0, scalar) buffer Indices { uint i[]; } indices;
layout(binding = 4, set = 0, scalar) buffer SubMeshes { SubMesh m[]; } subMeshes;
layout(binding = 5, set = 0, scalar) buffer Materials { GPUMaterial m[]; } materials;

layout(location = 0) rayPayloadInEXT RayPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed; 

hitAttributeEXT vec2 attribs;

layout(push_constant) uniform PushConstantsBlock { PushConstants pc; };

void main() {
    uint subMeshIdx = gl_InstanceCustomIndexEXT;
    SubMesh subMesh = subMeshes.m[subMeshIdx];
    ivec3 index = ivec3(indices.i[subMesh.firstIndex + 3 * gl_PrimitiveID], 
                        indices.i[subMesh.firstIndex + 3 * gl_PrimitiveID + 1], 
                        indices.i[subMesh.firstIndex + 3 * gl_PrimitiveID + 2]);

    Vertex v0 = vertices.v[index.x + subMesh.vertexOffset];
    Vertex v1 = vertices.v[index.y + subMesh.vertexOffset];
    Vertex v2 = vertices.v[index.z + subMesh.vertexOffset];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 worldPos0 = vec3(gl_ObjectToWorldEXT * vec4(v0.pos, 1.0));
    vec3 worldPos1 = vec3(gl_ObjectToWorldEXT * vec4(v1.pos, 1.0));
    vec3 worldPos2 = vec3(gl_ObjectToWorldEXT * vec4(v2.pos, 1.0));
    vec3 worldPos = worldPos0 * barycentrics.x + worldPos1 * barycentrics.y + worldPos2 * barycentrics.z;

    vec3 edge1 = worldPos1 - worldPos0;
    vec3 edge2 = worldPos2 - worldPos0;
    vec3 crossProd = cross(edge1, edge2);
    
    float areaSq = dot(crossProd, crossProd);
    if (areaSq < 1e-16) { prd.depth = 999; return; }
    vec3 geomNormal = crossProd * inversesqrt(areaSq);

    vec3 localNormal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    mat3 worldToObject3x3 = mat3(gl_WorldToObjectEXT[0].xyz, gl_WorldToObjectEXT[1].xyz, gl_WorldToObjectEXT[2].xyz);
    vec3 shadingNormal = localNormal * worldToObject3x3;
    
    if (dot(shadingNormal, shadingNormal) < 1e-6) shadingNormal = geomNormal;
    else shadingNormal = normalize(shadingNormal);

    vec3 V = normalize(-gl_WorldRayDirectionEXT);

    bool isBackface = dot(geomNormal, V) < 0.0;
    vec3 physN = isBackface ? -geomNormal : geomNormal;
    vec3 N = shadingNormal;
    if (dot(N, V) < 0.0) N = -N;

    vec3 albedo = materials.m[subMesh.materialIndex].baseColor.xyz;
    float roughness = materials.m[subMesh.materialIndex].roughness;
    float metallic = materials.m[subMesh.materialIndex].metallic;
    float transmission = materials.m[subMesh.materialIndex].transmission;
    float ior = materials.m[subMesh.materialIndex].ior;

    if (pc.matParams.w > 0.5f) {
        albedo = pc.albedo_Rough.xyz; 
        roughness = pc.albedo_Rough.w;
        metallic = pc.matParams.x;
        transmission = pc.matParams.y;
        ior = pc.matParams.z;
    }

    // 释放 albedo 的最大钳制阈值，允许纯净的透射能力
    albedo = clamp(albedo, vec3(0.01), vec3(1.0)); 
    roughness = clamp(roughness, 0.05, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    transmission = clamp(transmission, 0.0, 1.0);
    
    bool isInside = isBackface && (transmission > 0.01);

    float etaI = isInside ? ior : 1.0;
    float etaT = isInside ? 1.0 : ior;
    float eta = etaI / etaT;

    float F_dielectric = fresnelDielectric(dot(V, N), etaI, etaT);
    transmission *= (1.0 - metallic); 

    float eSpecular = mix(F_dielectric, 1.0, metallic);
    float eTransmit = (1.0 - F_dielectric) * transmission;
    float eDiffuse  = (1.0 - F_dielectric) * (1.0 - transmission) * (1.0 - metallic);
    
    float eTotal = eSpecular + eTransmit + eDiffuse;
    if (eTotal < 1e-6) { prd.depth = 999; return; }
    
    float pSpecular = eSpecular / eTotal;
    float pTransmit = eTransmit / eTotal;
    float pDiffuse  = eDiffuse  / eTotal;

    prd.emission = vec3(0.0); 
    
    vec3 sunDir = normalize(pc.lightDir_LgInt.xyz);
    float sunAngle = pc.envColor_LgAng.w;
    float sunIntensity = pc.lightDir_LgInt.w;

    if (transmission < 0.99 && !isInside) {
        LightSample ls = sampleSun(sunDir, sunAngle, sunIntensity, prd.seed);
        float NdotL = dot(N, ls.L);
        
        if (ls.pdf > 0.0 && NdotL > 0.0) {
            isShadowed = true;
            uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
            
            vec3 shadowOffset = dot(ls.L, geomNormal) > 0.0 ? geomNormal : -geomNormal;
            traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 1, offsetRay(worldPos, shadowOffset), 0.001, ls.L, ls.dist, 1);
            
            if (!isShadowed) {
                float bsdfPdf = pdfBRDF(V, ls.L, N, roughness, metallic, transmission);
                float misWeight = powerHeuristic(ls.pdf, bsdfPdf);
                vec3 bsdfEval = evalBRDF(V, ls.L, N, albedo, roughness, metallic, transmission);
                prd.emission = bsdfEval * ls.emission * misWeight * NdotL / ls.pdf;
            }
        }
    }

    vec3 nextDir;
    vec3 brdfWeight = vec3(0.0);
    vec3 rayOriginOffset = physN;
    bool isTransmissivePath = false; 

    float randVal = rnd(prd.seed);

    if (randVal < pSpecular) {
        vec3 H = importanceSampleGGX(vec2(rnd(prd.seed), rnd(prd.seed)), N, roughness);
        nextDir = reflect(-V, H);
        
        float VdotH = max(dot(V, H), 0.0);
        float NdotV = max(dot(N, V), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        
        float F_d = fresnelDielectric(VdotH, etaI, etaT);
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = mix(vec3(F_d), fresnelSchlick(VdotH, F0), metallic);
        float G = GeometrySmith(NdotV, max(dot(N, nextDir), 0.0), roughness);
        
        vec3 weight = (F * G * VdotH) / max(NdotV * NdotH, 1e-4);
        
        // 【补偿】将因粗糙度剧烈吸收的能量加回，防止变黑
        vec3 msCompensation = F * (1.0 - G) * roughness;
        weight += msCompensation;
        
        brdfWeight = weight / pSpecular; 
        rayOriginOffset = physN;
    } 
    else if (randVal < pSpecular + pTransmit) {
        vec3 H = importanceSampleGGX(vec2(rnd(prd.seed), rnd(prd.seed)), N, roughness);
        if (dot(-V, H) > 0.0) H = -H; 
        
        vec3 refracted = refract(-V, H, eta);
        if (dot(refracted, refracted) == 0.0) {
            nextDir = reflect(-V, H);
            rayOriginOffset = physN; 
        } else {
            nextDir = normalize(refracted);
            rayOriginOffset = -physN; 
            isTransmissivePath = true; 
        }
        brdfWeight = albedo * eTotal; 
    } 
    else {
        nextDir = cosineSampleHemisphere(N, prd.seed);
        // 化简后的纯正 diffuse 权重，避免除以小概率导致噪点爆炸
        brdfWeight = albedo * eTotal;
        rayOriginOffset = physN;
    }

    float cosMax = cos(sunAngle);
    if (dot(nextDir, sunDir) >= cosMax && !isTransmissivePath) {
        float bsdfPdf = pdfBRDF(V, nextDir, N, roughness, metallic, transmission);
        float neePdf = 1.0 / (2.0 * PI * (1.0 - cosMax));
        float misWeight_BRDF = powerHeuristic(bsdfPdf, neePdf);
        brdfWeight *= misWeight_BRDF;
    }

    if (rayOriginOffset == physN && dot(nextDir, physN) < 0.0) nextDir = reflect(nextDir, physN);
    else if (rayOriginOffset == -physN && dot(nextDir, physN) > 0.0) nextDir = reflect(nextDir, physN);

    if (isInside && transmission > 0.01) {
        vec3 absorption = (1.0 - albedo) * 2.0; 
        prd.throughputWeight *= exp(-absorption * gl_HitTEXT);
    }

    prd.throughputWeight *= brdfWeight;

    // 【补偿】大幅延后透射光线的轮盘赌判定
    // 玻璃内部全反射极多，3 次弹射大概率还没穿透出去，给予玻璃光线至少 8 次以上的保底存活期
    uint rrDepth = (transmission > 0.01) ? 8 : 3;
    if (prd.depth > rrDepth) {
        float pRR = max(prd.throughputWeight.r, max(prd.throughputWeight.g, prd.throughputWeight.b));
        pRR = clamp(pRR, 0.05, 0.95);
        if (rnd(prd.seed) > pRR) {
            prd.depth = 999;
            return;
        }
        prd.throughputWeight /= pRR;
    }

    prd.rayOrigin = offsetRay(worldPos, rayOriginOffset);
    prd.rayDir = nextDir;
    prd.depth += 1;
}