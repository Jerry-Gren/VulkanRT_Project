#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require

const float PI = 3.14159265359;
const float EPSILON = 0.0001;

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

struct SubMesh {
    uint firstIndex;
    uint indexCount;
    int vertexOffset;
    uint materialIndex;
};

struct GPUMaterial {
    vec4 baseColor;
    float metallic;
    float roughness;
    int textureID;
    int padding;
};

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 0, scalar) buffer Vertices { Vertex v[]; } vertices;
layout(binding = 3, set = 0, scalar) buffer Indices { uint i[]; } indices;
layout(binding = 4, set = 0, scalar) buffer SubMeshes { SubMesh m[]; } subMeshes;
layout(binding = 5, set = 0, scalar) buffer Materials { GPUMaterial m[]; } materials;

struct RayPayload {
    vec3 emission;
    vec3 throughputWeight;
    uint depth;
    uint seed;
    vec3 rayOrigin;
    vec3 rayDir;
};

layout(location = 0) rayPayloadInEXT RayPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

hitAttributeEXT vec2 attribs;

layout(push_constant) uniform PushConstants {
    vec4 cameraPos;
    vec4 cameraDir;
    vec4 cameraUp;
    vec4 cameraRight;
    vec4 projParams;
    vec4 envConfig; 
    vec4 lightDir;     
    vec4 lightColor;   
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
    float x = cos(phi) * sqrt(r2);
    float y = sin(phi) * sqrt(r2);
    return buildTBN(n) * vec3(x, y, z);
}

vec3 importanceSampleGGX(vec2 xi, vec3 n, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 h = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    return buildTBN(n) * h;
}

// 工业级修复 1：引入考虑粗糙度的菲涅尔方程，消除模型边缘的“廉价玻璃光晕”
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) * geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

float cosineSampleHemispherePDF(float NdotL) {
    return NdotL / PI;
}

float importanceSampleGGXPDF(float NdotH, float VdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    float D = a2 / (PI * denom * denom);
    return (D * NdotH) / (4.0 * VdotH);
}

float powerHeuristic(float a, float b) {
    float a2 = a * a;
    float b2 = b * b;
    return a2 / (a2 + b2);
}

vec3 offsetRay(const vec3 p, const vec3 n) {
    const float intScale   = 256.0f;
    const float floatScale = 1.0f / 65536.0f;
    const float origin     = 1.0f / 32.0f;
    ivec3 of_i = ivec3(intScale * n.x, intScale * n.y, intScale * n.z);
    vec3 p_i = vec3(
        intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0.0f) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0.0f) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0.0f) ? -of_i.z : of_i.z))
    );
    return vec3(
        abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x,
        abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y,
        abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z
    );
}

void main() {
    uint subMeshIdx = gl_InstanceCustomIndexEXT;
    SubMesh subMesh = subMeshes.m[subMeshIdx];
    ivec3 index = ivec3(
        indices.i[subMesh.firstIndex + 3 * gl_PrimitiveID], 
        indices.i[subMesh.firstIndex + 3 * gl_PrimitiveID + 1], 
        indices.i[subMesh.firstIndex + 3 * gl_PrimitiveID + 2]
    );

    Vertex v0 = vertices.v[index.x + subMesh.vertexOffset];
    Vertex v1 = vertices.v[index.y + subMesh.vertexOffset];
    Vertex v2 = vertices.v[index.z + subMesh.vertexOffset];

    const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 worldPos0 = vec3(gl_ObjectToWorldEXT * vec4(v0.pos, 1.0));
    vec3 worldPos1 = vec3(gl_ObjectToWorldEXT * vec4(v1.pos, 1.0));
    vec3 worldPos2 = vec3(gl_ObjectToWorldEXT * vec4(v2.pos, 1.0));
    vec3 worldPos = worldPos0 * barycentrics.x + worldPos1 * barycentrics.y + worldPos2 * barycentrics.z;

    vec3 geomNormal = normalize(cross(worldPos1 - worldPos0, worldPos2 - worldPos0));
    if (dot(gl_WorldRayDirectionEXT, geomNormal) > 0.0) geomNormal = -geomNormal;

    vec3 shadingNormal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
    shadingNormal = normalize(vec3(gl_ObjectToWorldEXT * vec4(shadingNormal, 0.0)));
    if (dot(gl_WorldRayDirectionEXT, shadingNormal) > 0.0) shadingNormal = -shadingNormal;

    GPUMaterial mat = materials.m[subMesh.materialIndex];
    vec3 albedo = mat.baseColor.xyz;
    float roughness = clamp(mat.roughness, 0.01, 1.0);
    float metallic = clamp(mat.metallic, 0.0, 1.0);

    vec3 V = normalize(-gl_WorldRayDirectionEXT);
    vec3 N = shadingNormal;
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float pSpecular = clamp(max(max(F0.x, F0.y), F0.z) + metallic * 0.5, 0.1, 0.9);
    float pDiffuse = 1.0 - pSpecular;

    prd.emission = vec3(0.0);
    vec3 safeOrigin = offsetRay(worldPos, geomNormal);

    // ==========================================
    // Next Event Estimation (显式光源采样)
    // ==========================================
    vec3 Ld = vec3(0.0);
    if (pc.lightColor.xyz != vec3(0.0)) {
        vec2 xi = vec2(rnd(prd.seed), rnd(prd.seed));
        vec3 lightCenterDir = normalize(pc.lightDir.xyz);
        float angleRadius = pc.lightDir.w;
        
        float cosThetaCone = mix(cos(angleRadius), 1.0, xi.x);
        float sinThetaCone = sqrt(1.0 - cosThetaCone * cosThetaCone);
        float phiCone = 2.0 * PI * xi.y;
        vec3 lLocal = vec3(cos(phiCone) * sinThetaCone, sin(phiCone) * sinThetaCone, cosThetaCone);
        vec3 L_nee = buildTBN(lightCenterDir) * lLocal;
        
        float NdotL_nee = max(dot(N, L_nee), 0.0);
        float dotGeomL = dot(geomNormal, L_nee);
        
        if (NdotL_nee > 0.0) {
            vec3 shadowOrigin = safeOrigin;
            
            // 工业级修复 2：Ray Tracing Gems 方案。当着色法线认为被照亮，但几何法线产生遮挡时，
            // 稍稍推进射线起点以避免自交，而不是使用 smoothstep 产生廉价的暗边勾线。
            if (dotGeomL <= 0.0) {
                shadowOrigin += L_nee * 0.005; 
            }

            isShadowed = true;
            traceRayEXT(topLevelAS, 
                        gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT, 
                        0xFF, 0, 0, 1, 
                        shadowOrigin, EPSILON, L_nee, 10000.0, 1);
            
            if (!isShadowed) {
                vec3 H_nee = normalize(V + L_nee);
                float NdotV = max(dot(N, V), 0.0);
                float VdotH = max(dot(V, H_nee), 0.0);
                float NdotH = max(dot(N, H_nee), 0.0);

                vec3 F = fresnelSchlickRoughness(VdotH, F0, roughness);
                float G = geometrySmith(N, V, L_nee, roughness);
                
                vec3 F_diffuse = fresnelSchlickRoughness(NdotV, F0, roughness);
                vec3 kd = (1.0 - F_diffuse) * (1.0 - metallic);
                vec3 diffuse = kd * albedo / PI;
                
                float a2 = roughness * roughness;
                float denomD = (NdotH * NdotH * (a2 - 1.0) + 1.0);
                float D = a2 / (PI * denomD * denomD);
                vec3 specular = (F * G * D) / max(4.0 * NdotV * NdotL_nee, 0.001);

                vec3 brdf = diffuse + specular;

                float pdf_nee = 1.0 / (2.0 * PI * (1.0 - cos(angleRadius)));
                float pdf_brdf = pDiffuse * cosineSampleHemispherePDF(NdotL_nee) +
                                 pSpecular * importanceSampleGGXPDF(NdotH, VdotH, roughness);
                                
                float misWeight = powerHeuristic(pdf_nee, pdf_brdf);
                
                Ld = (brdf * NdotL_nee * pc.lightColor.xyz * misWeight) / max(pdf_nee, EPSILON); 
            }
        }
    }
    prd.emission = Ld;

    // ==========================================
    // BRDF 间接光采样 (俄罗斯轮盘赌路径)
    // ==========================================
    vec3 nextDir;
    vec3 brdfWeight = vec3(0.0);

    if (rnd(prd.seed) < pSpecular) {
        vec2 xi = vec2(rnd(prd.seed), rnd(prd.seed));
        vec3 H = importanceSampleGGX(xi, N, roughness);
        nextDir = reflect(-V, H);
        
        float NdotL = dot(N, nextDir);
        float NdotV = max(dot(N, V), 0.0);
        
        if (NdotL > 0.0 && NdotV > 0.0) {
            float VdotH = max(dot(V, H), 0.0);
            float NdotH = max(dot(N, H), 0.0);
            
            vec3 F = fresnelSchlickRoughness(VdotH, F0, roughness);
            float G = geometrySmith(N, V, nextDir, roughness);
            
            brdfWeight = (F * G * VdotH) / max(NdotV * NdotH, EPSILON);
            brdfWeight /= pSpecular;
        }
    } else {
        nextDir = cosineSampleHemisphere(N, prd.seed);
        float NdotL = max(dot(N, nextDir), 0.0);
        
        if (NdotL > 0.0) {
            vec3 F_diffuse = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
            vec3 kd = (1.0 - F_diffuse) * (1.0 - metallic);
            brdfWeight = kd * albedo;
            brdfWeight /= pDiffuse;
        }
    }

    // 工业级修复 3：能量守恒的几何边界处理。禁止粗暴地剔除钻入几何体的光线（导致暗区边缘死黑），
    // 而是将其强制翻转回表面外，这能在低多边形网格边缘保持能量守恒，消除黑斑。
    if (dot(nextDir, geomNormal) <= 0.0) {
        nextDir = nextDir - 2.0 * dot(nextDir, geomNormal) * geomNormal;
    }

    prd.throughputWeight = brdfWeight;
    prd.rayOrigin = safeOrigin;
    prd.rayDir = nextDir;
    prd.depth += 1;
}