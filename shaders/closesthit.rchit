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

    albedo = clamp(albedo, vec3(0.01), vec3(1.0)); 
    roughness = clamp(roughness, 0.01, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    transmission = clamp(transmission, 0.0, 1.0);
    
    bool isInside = isBackface && (transmission > 0.01);

    if (isInside) {
        roughness = min(roughness, 0.15); 
    }

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

    if (!isInside) {
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
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    if (randVal < pSpecular) {
        vec3 H = sampleGGXVNDF(V, N, roughness, prd.seed);
        nextDir = reflect(-V, H);
        
        float NdotV = max(dot(N, V), 1e-4);
        float NdotL = dot(N, nextDir);
        
        // 【核心修复 1】：剔除射向地平线以下的无效反射。丢弃它们代表着正确的 G 项衰减。
        if (NdotL <= 0.0) {
            brdfWeight = vec3(0.0);
        } else {
            float VdotH = max(dot(V, H), 0.0);
            float F_d = fresnelDielectric(VdotH, etaI, etaT);
            vec3 F = mix(vec3(F_d), fresnelSchlick(VdotH, F0), metallic);
            
            // 【核心修复 2】：使用严格的 VNDF 数学约分权重 G2/G1，不瞎乘放大系数，根除过曝爆炸！
            vec3 singleScatterWeight = F * G2_over_G1(NdotV, NdotL, roughness);
            
            brdfWeight = singleScatterWeight / pSpecular; 
            rayOriginOffset = physN;
        }
    } 
    else if (randVal < pSpecular + pTransmit) {
        vec3 H = sampleGGXVNDF(V, N, roughness, prd.seed);
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
        brdfWeight = vec3(eTotal); 
    } 
    else {
        nextDir = cosineSampleHemisphere(N, prd.seed);
        brdfWeight = albedo * eTotal;
        rayOriginOffset = physN;
    }

    // 同步限制 cosMax 防止 neePdf 除零溢出
    float cosMax = min(cos(sunAngle), 0.999999);
    if (dot(nextDir, sunDir) >= cosMax && !isTransmissivePath && dot(N, nextDir) > 0.0) {
        float bsdfPdf = pdfBRDF(V, nextDir, N, roughness, metallic, transmission);
        float neePdf = 1.0 / (2.0 * PI * (1.0 - cosMax));
        float misWeight_BRDF = powerHeuristic(bsdfPdf, neePdf);
        brdfWeight *= misWeight_BRDF;
    }

    // 【核心修复 3】：彻底删除了之前的 nextDir = reflect(nextDir, physN) 镜像 Hack。不再凭空制造漫反射！

    if (isInside && transmission > 0.01) {
        vec3 absorption = (1.0 - albedo) * 2.0; 
        prd.throughputWeight *= exp(-absorption * gl_HitTEXT);
    }

    prd.throughputWeight *= brdfWeight;

    uint rrDepth = (transmission > 0.01) ? 15 : 4;
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