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
layout(binding = 6, set = 0, scalar) buffer Lights { GPULight l[]; } lights;

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
    mat3 worldToObject3x3 = mat3(
        gl_WorldToObjectEXT[0].xyz, gl_WorldToObjectEXT[1].xyz, gl_WorldToObjectEXT[2].xyz
    );
    vec3 shadingNormal = localNormal * worldToObject3x3;
    
    if (dot(shadingNormal, shadingNormal) < 1e-6) shadingNormal = geomNormal;
    else shadingNormal = normalize(shadingNormal);

    if (dot(geomNormal, shadingNormal) < 0.0) shadingNormal = -shadingNormal;

    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    bool isInside = dot(geomNormal, V) < 0.0;
    
    vec3 physN = isInside ? -geomNormal : geomNormal;
    vec3 N = isInside ? -shadingNormal : shadingNormal;
    if (dot(N, V) < 0.001) N = physN;

    vec3 albedo = materials.m[subMesh.materialIndex].baseColor.xyz;
    float roughness = clamp(materials.m[subMesh.materialIndex].roughness, 0.01, 1.0);
    float metallic = clamp(materials.m[subMesh.materialIndex].metallic, 0.0, 1.0);
    float transmission = clamp(materials.m[subMesh.materialIndex].transmission, 0.0, 1.0);
    float ior = materials.m[subMesh.materialIndex].ior;

    if (pc.matParams.w > 0.5f) {
        albedo = pc.albedo_Rough.xyz; roughness = clamp(pc.albedo_Rough.w, 0.01, 1.0);
        metallic = clamp(pc.matParams.x, 0.0, 1.0); transmission = clamp(pc.matParams.y, 0.0, 1.0);
        ior = pc.matParams.z;
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

    prd.emission = vec3(0.0);
    vec3 nextDir;
    vec3 brdfWeight = vec3(0.0);
    vec3 rayOriginOffset = physN;

    float randVal = rnd(prd.seed);

    if (randVal < pSpecular) {
        vec3 H = importanceSampleGGX(vec2(rnd(prd.seed), rnd(prd.seed)), N, roughness);
        nextDir = reflect(-V, H);
        float VdotH = max(dot(V, H), 0.0);
        vec3 F_metal = albedo + (1.0 - albedo) * pow(1.0 - VdotH, 5.0);
        brdfWeight = mix(vec3(1.0), F_metal, metallic) * eTotal; 
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
        }
        brdfWeight = albedo * eTotal; 
    } 
    else {
        nextDir = cosineSampleHemisphere(N, prd.seed);
        brdfWeight = albedo * eTotal;
    }

    if (rayOriginOffset == physN && dot(nextDir, physN) < 0.0) nextDir = reflect(nextDir, physN);
    else if (rayOriginOffset == -physN && dot(nextDir, physN) > 0.0) nextDir = reflect(nextDir, physN);

    if (isInside && transmission > 0.01) {
        vec3 absorption = (1.0 - albedo) * 2.0; 
        prd.throughputWeight *= exp(-absorption * gl_HitTEXT);
    }

    prd.throughputWeight *= brdfWeight;
    prd.rayOrigin = offsetRay(worldPos, rayOriginOffset);
    prd.rayDir = nextDir;
    prd.depth += 1;
}