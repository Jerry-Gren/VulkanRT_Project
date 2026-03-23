#ifndef RANDOM_GLSL
#define RANDOM_GLSL

uint pcg_hash(uint seed) {
    uint state = seed * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rnd(inout uint seed) {
    seed = pcg_hash(seed);
    return float(seed) / 4294967296.0;
}

#endif