#ifndef MATH_GLSL
#define MATH_GLSL

mat3 buildTBN(vec3 n) {
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(up, n));
    vec3 b = cross(n, t);
    return mat3(t, b, n);
}

// 避免自交的自适应光线偏移
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

#endif