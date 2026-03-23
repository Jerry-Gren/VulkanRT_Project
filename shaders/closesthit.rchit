#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require

const float PI = 3.14159265359;

struct Vertex { vec3 pos; vec3 normal; vec2 uv; };
struct SubMesh { uint firstIndex; uint indexCount; int vertexOffset; uint materialIndex; };
struct GPUMaterial { vec4 baseColor; float metallic; float roughness; float transmission; float ior; int textureID; int pad[3]; };

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0, scalar) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 3, set = 0, scalar) buffer Indices { uint i[]; } indices;
layout(binding = 4, set = 0, scalar) buffer SubMeshes { SubMesh m[]; } subMeshes;
layout(binding = 5, set = 0, scalar) buffer Materials { GPUMaterial m[]; } materials;

struct RayPayload { vec3 emission; vec3 throughputWeight; uint depth; uint seed; vec3 rayOrigin; vec3 rayDir; };
layout(location = 0) rayPayloadInEXT RayPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

hitAttributeEXT vec2 attribs;

layout(push_constant) uniform PushConstants {
    vec4 camPos_Fov;       
    vec4 camDir_Aspect;    
    vec4 camUp_Frame;      
    vec4 camRight_EnvInt;  
    vec4 envColor_LgAng;   
    vec4 lightDir_LgInt;   
    vec4 albedo_Rough;     
    vec4 matParams;        
} pc;

uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rnd(inout uint seed) {
    seed = pcg_hash(seed);
    return float(seed) / 4294967296.0;
}

mat3 buildTBN(vec3 n) {
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(up, n));
    vec3 b = cross(n, t);
    return mat3(t, b, n);
}

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

vec3 offsetRay(const vec3 p, const vec3 n) {
    const float intScale = 256.0f; const float floatScale = 1.0f / 65536.0f; const float origin = 1.0f / 32.0f;
    ivec3 of_i = ivec3(intScale * n.x, intScale * n.y, intScale * n.z);
    vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0.0f) ? -of_i.x : of_i.x)),
                    intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0.0f) ? -of_i.y : of_i.y)),
                    intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0.0f) ? -of_i.z : of_i.z)));
    return vec3(abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x,
                abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y,
                abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

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
        gl_WorldToObjectEXT[0].xyz,
        gl_WorldToObjectEXT[1].xyz,
        gl_WorldToObjectEXT[2].xyz
    );
    vec3 shadingNormal = localNormal * worldToObject3x3;
    
    if (dot(shadingNormal, shadingNormal) < 1e-6) {
        shadingNormal = geomNormal;
    } else {
        shadingNormal = normalize(shadingNormal);
    }

    if (dot(geomNormal, shadingNormal) < 0.0) {
        shadingNormal = -shadingNormal;
    }

    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    
    // 【修复 1】：内外判定必须且只能基于真实的物理边界 (geomNormal)
    bool isInside = dot(geomNormal, V) < 0.0;
    
    vec3 physN = isInside ? -geomNormal : geomNormal;
    vec3 N = isInside ? -shadingNormal : shadingNormal;
    
    // 阴影终结者初步防护：如果插值法线背离了观察方向，强制回调到物理法线
    if (dot(N, V) < 0.001) {
        N = physN;
    }

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
    
    roughness = clamp(roughness, 0.01, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);
    transmission = clamp(transmission, 0.0, 1.0);

    float etaI = isInside ? ior : 1.0;
    float etaT = isInside ? 1.0 : ior;
    float eta = etaI / etaT;

    float F_dielectric = fresnelDielectric(dot(V, N), etaI, etaT);
    transmission *= (1.0 - metallic); 

    // 【修复 2】：解耦金属与电介质的能量。金属不使用电介质菲涅尔，其反射概率强制逼近 1.0
    float eSpecular = mix(F_dielectric, 1.0, metallic);
    float eTransmit = (1.0 - F_dielectric) * transmission;
    float eDiffuse  = (1.0 - F_dielectric) * (1.0 - transmission) * (1.0 - metallic);
    
    float eTotal = eSpecular + eTransmit + eDiffuse;
    if (eTotal < 1e-6) {
        prd.depth = 999; 
        return;
    }
    
    float pSpecular = eSpecular / eTotal;
    float pTransmit = eTransmit / eTotal;
    float pDiffuse  = eDiffuse  / eTotal;

    prd.emission = vec3(0.0);
    vec3 nextDir;
    vec3 brdfWeight = vec3(0.0);
    vec3 rayOriginOffset = physN;

    float randVal = rnd(prd.seed);

    if (randVal < pSpecular) {
        vec3 H = importanceSampleGGX(vec2(rnd(prd.seed), rnd(prd.seed)), N, roughness);
        nextDir = reflect(-V, H);
        
        // 【修复 3】：金属微表面 Schlick 菲涅尔，恢复金属高光与本色
        float VdotH = max(dot(V, H), 0.0);
        vec3 F_metal = albedo + (1.0 - albedo) * pow(1.0 - VdotH, 5.0);
        vec3 specColor = mix(vec3(1.0), F_metal, metallic);
        
        brdfWeight = specColor * eTotal; 
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

    // 【修复 4】：防自交射线弯曲 (Ray Bending)。
    // 如果因法线平滑插值导致射出的光线实际上穿入了物理多边形内部，不要杀死它（会产生黑线），
    // 而是像台球一样让它从物理多边形表面“弹”起，强行纠正回半球外。
    if (rayOriginOffset == physN && dot(nextDir, physN) < 0.0) {
        nextDir = reflect(nextDir, physN);
    } else if (rayOriginOffset == -physN && dot(nextDir, physN) > 0.0) {
        nextDir = reflect(nextDir, physN);
    }

    if (isInside && transmission > 0.01) {
        vec3 absorption = (1.0 - albedo) * 2.0; 
        prd.throughputWeight *= exp(-absorption * gl_HitTEXT);
    }

    prd.throughputWeight *= brdfWeight;
    prd.rayOrigin = offsetRay(worldPos, rayOriginOffset);
    prd.rayDir = nextDir;
    prd.depth += 1;
}