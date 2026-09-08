/**
 * @file math3d.h
 * @brief Self-contained 3D vector and 4x4 matrix mathematics for Vulkan in C17.
 *
 * Implements column-major matrix arithmetic compatible with GLSL std140 layout
 * and Vulkan clip space coordinates ([0, 1] depth range, inverted Y).
 */

#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

typedef struct vec2 {
    float x;
    float y;
} vec2;

typedef struct vec3 {
    float x;
    float y;
    float z;
} vec3;

typedef struct vec4 {
    float x;
    float y;
    float z;
    float w;
} vec4;

/**
 * @brief 4x4 column-major matrix.
 * Elements accessed as m[column][row].
 */
typedef struct mat4 {
    float m[4][4];
} mat4;

static inline float deg_to_rad(float degrees) {
    return degrees * (3.14159265358979323846f / 180.0f);
}

static inline vec3 vec3_create(float x, float y, float z) {
    vec3 v = {x, y, z};
    return v;
}

static inline vec3 vec3_add(vec3 a, vec3 b) {
    vec3 v = {a.x + b.x, a.y + b.y, a.z + b.z};
    return v;
}

static inline vec3 vec3_sub(vec3 a, vec3 b) {
    vec3 v = {a.x - b.x, a.y - b.y, a.z - b.z};
    return v;
}

static inline vec3 vec3_scale(vec3 a, float s) {
    vec3 v = {a.x * s, a.y * s, a.z * s};
    return v;
}

static inline float vec3_dot(vec3 a, vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline vec3 vec3_cross(vec3 a, vec3 b) {
    vec3 v = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return v;
}

static inline float vec3_length(vec3 v) {
    return sqrtf(vec3_dot(v, v));
}

static inline vec3 vec3_normalize(vec3 v) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        float inv = 1.0f / len;
        return vec3_scale(v, inv);
    }
    vec3 zero = {0.0f, 0.0f, 0.0f};
    return zero;
}

static inline mat4 mat4_identity(void) {
    mat4 res;
    memset(&res, 0, sizeof(mat4));
    res.m[0][0] = 1.0f;
    res.m[1][1] = 1.0f;
    res.m[2][2] = 1.0f;
    res.m[3][3] = 1.0f;
    return res;
}

static inline mat4 mat4_mul(mat4 a, mat4 b) {
    mat4 res;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            res.m[c][r] = a.m[0][r] * b.m[c][0] +
                          a.m[1][r] * b.m[c][1] +
                          a.m[2][r] * b.m[c][2] +
                          a.m[3][r] * b.m[c][3];
        }
    }
    return res;
}

static inline mat4 mat4_translate(mat4 m, vec3 v) {
    mat4 t = mat4_identity();
    t.m[3][0] = v.x;
    t.m[3][1] = v.y;
    t.m[3][2] = v.z;
    return mat4_mul(m, t);
}

static inline mat4 mat4_rotate(mat4 m, float angle_rad, vec3 axis) {
    vec3 a = vec3_normalize(axis);
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);
    float one_c = 1.0f - c;

    mat4 r = mat4_identity();
    r.m[0][0] = c + a.x * a.x * one_c;
    r.m[0][1] = a.y * a.x * one_c + a.z * s;
    r.m[0][2] = a.z * a.x * one_c - a.y * s;

    r.m[1][0] = a.x * a.y * one_c - a.z * s;
    r.m[1][1] = c + a.y * a.y * one_c;
    r.m[1][2] = a.z * a.y * one_c + a.x * s;

    r.m[2][0] = a.x * a.z * one_c + a.y * s;
    r.m[2][1] = a.y * a.z * one_c - a.x * s;
    r.m[2][2] = c + a.z * a.z * one_c;

    return mat4_mul(m, r);
}

static inline mat4 mat4_scale(mat4 m, vec3 s) {
    mat4 scale_mat = mat4_identity();
    scale_mat.m[0][0] = s.x;
    scale_mat.m[1][1] = s.y;
    scale_mat.m[2][2] = s.z;
    return mat4_mul(m, scale_mat);
}

/**
 * @brief Constructs a perspective projection matrix tailored for Vulkan NDC.
 *
 * Vulkan NDC uses Z in [0, 1] and an inverted Y-axis relative to OpenGL.
 */
static inline mat4 mat4_perspective(float fovy_rad, float aspect, float z_near, float z_far) {
    float tan_half_fovy = tanf(fovy_rad * 0.5f);
    mat4 res;
    memset(&res, 0, sizeof(mat4));

    res.m[0][0] = 1.0f / (aspect * tan_half_fovy);
    res.m[1][1] = -1.0f / tan_half_fovy; /* Invert Y for Vulkan clip space */
    res.m[2][2] = z_far / (z_near - z_far);
    res.m[2][3] = -1.0f;
    res.m[3][2] = -(z_far * z_near) / (z_far - z_near);

    return res;
}

/**
 * @brief Constructs a right-handed view matrix looking from eye towards center.
 */
static inline mat4 mat4_look_at(vec3 eye, vec3 center, vec3 up) {
    vec3 f = vec3_normalize(vec3_sub(center, eye));
    vec3 s = vec3_normalize(vec3_cross(f, up));
    vec3 u = vec3_cross(s, f);

    mat4 res = mat4_identity();
    res.m[0][0] = s.x;
    res.m[1][0] = s.y;
    res.m[2][0] = s.z;
    res.m[3][0] = -vec3_dot(s, eye);

    res.m[0][1] = u.x;
    res.m[1][1] = u.y;
    res.m[2][1] = u.z;
    res.m[3][1] = -vec3_dot(u, eye);

    res.m[0][2] = -f.x;
    res.m[1][2] = -f.y;
    res.m[2][2] = -f.z;
    res.m[3][2] = vec3_dot(f, eye);

    return res;
}

#endif /* MATH3D_H */
