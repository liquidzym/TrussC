#pragma once
// AssimpConvert.h — Assimp ↔ TrussC type conversion (no GLM, no Assimp in runtime)
#include <TrussC.h>
#include <assimp/vector3.h>
#include <assimp/quaternion.h>
#include <assimp/matrix4x4.h>
#include <assimp/color4.h>
#include <assimp/types.h>

namespace tcx::assimp {

// Vector conversions
inline tc::Vec2 toVec2(const aiVector2D& v) { return tc::Vec2(v.x, v.y); }
inline tc::Vec3 toVec3(const aiVector3D& v) { return tc::Vec3(v.x, v.y, v.z); }
inline tc::Vec4 toVec4(const aiVector3D& v, float w = 1.0f) { return tc::Vec4(v.x, v.y, v.z, w); }
inline tc::Color toColor(const aiColor3D& c, float a = 1.0f) { return tc::Color(c.r, c.g, c.b, a); }
inline tc::Color toColor(const aiColor4D& c) { return tc::Color(c.r, c.g, c.b, c.a); }

// Quaternion conversion (Assimp aiQuaternion is (w, x, y, z))
inline tc::Quaternion toQuat(const aiQuaternion& q) {
    return tc::Quaternion(q.w, q.x, q.y, q.z);
}
inline tc::Quaternion toQuaternion(const aiQuaternion& q) { return toQuat(q); }

// Matrix conversion: aiMatrix4x4 and tc::Mat4 are both used as row-major
// matrices in their C++ math APIs. Keep row/column positions unchanged here;
// TrussC transposes internally only when uploading to GPU shaders.
inline tc::Mat4 toMat4(const aiMatrix4x4& m) {
    return tc::Mat4(
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4
    );
}

// Reverse (internal use only)
inline aiVector3D toAiVec(const tc::Vec3& v) { return aiVector3D(v.x, v.y, v.z); }
inline aiVector3D toAiVector3D(const tc::Vec3& v) { return toAiVec(v); }
inline aiQuaternion toAiQuat(const tc::Quaternion& q) {
    return aiQuaternion(q.w, q.x, q.y, q.z);
}
inline aiQuaternion toAiQuaternion(const tc::Quaternion& q) { return toAiQuat(q); }
inline aiMatrix4x4 toAiMat4(const tc::Mat4& m) {
    aiMatrix4x4 r;
    r.a1 = m.m[0];  r.a2 = m.m[1];  r.a3 = m.m[2];  r.a4 = m.m[3];
    r.b1 = m.m[4];  r.b2 = m.m[5];  r.b3 = m.m[6];  r.b4 = m.m[7];
    r.c1 = m.m[8];  r.c2 = m.m[9];  r.c3 = m.m[10]; r.c4 = m.m[11];
    r.d1 = m.m[12]; r.d2 = m.m[13]; r.d3 = m.m[14]; r.d4 = m.m[15];
    return r;
}

} // namespace tcx::assimp
