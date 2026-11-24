
// ================================================================================
// --------------------------------------------------
// * Telegram Bot: @imguisource_bot
// * ImGui Source: t.me/ImGui_Source_FIXNEY
// * shit src 🧨: t.me/il2cpp_src
// --------------------------------------------------
// ================================================================================

#pragma once
#include <cmath>
#ifndef RAD2DEG
#define RAD2DEG(x) ((float)(x) * (180.f / 3.14159265358979323846f))
#endif
struct Vector2 {
  float x, y;

  Vector2() : x(0), y(0) {}
  Vector2(float x, float y) : x(x), y(y) {}

  Vector2 operator+(const Vector2 &o) const { return {x + o.x, y + o.y}; }

  Vector2 operator-(const Vector2 &o) const { return {x - o.x, y - o.y}; }

  Vector2 operator*(float s) const { return {x * s, y * s}; }

  Vector2 operator/(float s) const { return {x / s, y / s}; }

  float Length() const { return std::sqrt(x * x + y * y); }

  float LengthSquared() const { return x * x + y * y; }

  static float Dot(const Vector2 &a, const Vector2 &b) {
    return a.x * b.x + a.y * b.y;
  }
  float Distance(const Vector2 &other) const {
    return (*this - other).Length();
  }

  float DistanceSquared(const Vector2 &other) const {
    return (*this - other).LengthSquared();
  }
};
inline Vector2 &operator+=(Vector2 &a, const Vector2 &b) {
  a.x += b.x;
  a.y += b.y;
  return a;
}

// Vector2 -= Vector2
inline Vector2 &operator-=(Vector2 &a, const Vector2 &b) {
  a.x -= b.x;
  a.y -= b.y;
  return a;
}

// float * Vector2
inline Vector2 operator*(float s, const Vector2 &v) {
  return Vector2(v.x * s, v.y * s);
}

struct Vector3 {
  float x, y, z;

  Vector3() : x(0), y(0), z(0) {}
  Vector3(float x, float y, float z) : x(x), y(y), z(z) {}

  // Addition
  Vector3 operator+(const Vector3 &other) const {
    return Vector3(x + other.x, y + other.y, z + other.z);
  }

  Vector3 &operator+=(const Vector3 &other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
  }

  // Subtraction
  Vector3 operator-(const Vector3 &other) const {
    return Vector3(x - other.x, y - other.y, z - other.z);
  }

  Vector3 &operator-=(const Vector3 &other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
  }

  // Scalar multiplication
  Vector3 operator*(float scalar) const {
    return Vector3(x * scalar, y * scalar, z * scalar);
  }

  Vector3 &operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    z *= scalar;
    return *this;
  }

  // Component-wise multiplication (for scale)
  Vector3 operator*(const Vector3 &other) const {
    return Vector3(x * other.x, y * other.y, z * other.z);
  }

  // Division
  Vector3 operator/(float scalar) const {
    return Vector3(x / scalar, y / scalar, z / scalar);
  }

  Vector3 operator/(const Vector3 &other) const {
    return Vector3(x / other.x, y / other.y, z / other.z);
  }

  // Cross product (needed for quaternion rotation)
  Vector3 Cross(const Vector3 &other) const {
    return Vector3(y * other.z - z * other.y, z * other.x - x * other.z,
                   x * other.y - y * other.x);
  }

  // Dot product
  float Dot(const Vector3 &other) const {
    return x * other.x + y * other.y + z * other.z;
  }

  // Magnitude
  float Magnitude() const { return sqrtf(x * x + y * y + z * z); }

  // Distance
  float Distance(const Vector3 &other) const {
    return (*this - other).Magnitude();
  }

  // Normalize
  Vector3 Normalized() const {
    float mag = Magnitude();
    if (mag > 0.00001f) {
      return Vector3(x / mag, y / mag, z / mag);
    }
    return Vector3(0, 0, 0);
  }

  // Comparison
  bool operator==(const Vector3 &other) const {
    return x == other.x && y == other.y && z == other.z;
  }

  bool operator!=(const Vector3 &other) const { return !(*this == other); }

  bool Empty() const { return x == 0 && y == 0 && z == 0; }
  bool IsValid() const {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
  }
  float Length() const { return sqrtf(x * x + y * y + z * z); }
  float LengthSquared() const { return x * x + y * y + z * z; }

  // Normalize and return normalized vector
  Vector3 Normalize() const {
    float mag = Length();
    if (mag > 0.00001f) {
      return Vector3(x / mag, y / mag, z / mag);
    }
    return Vector3(0, 0, 0);
  }

  // Array access operator
  float operator[](int i) const {
    if (i == 0)
      return x;
    if (i == 1)
      return y;
    return z;
  }

  // Inside struct Vector2
  static float Dot(const Vector2 &a, const Vector2 &b) {
    return a.x * b.x + a.y * b.y;
  }
};

// Helper: Scalar * Vector3
inline Vector3 operator*(float scalar, const Vector3 &vec) {
  return vec * scalar;
}
struct Vector4 {
  float x, y, z, w;

  // Constructors
  Vector4() : x(0), y(0), z(0), w(1) {}
  Vector4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

  // Quaternion conjugate inverse shit
  Vector4 conjugate() const { return Vector4(-x, -y, -z, w); }

  // Quaternion multiplication combining rotations
  Vector4 operator*(const Vector4 &q) const {
    return Vector4(w * q.x + x * q.w + y * q.z - z * q.y,
                   w * q.y - x * q.z + y * q.w + z * q.x,
                   w * q.z + x * q.y - y * q.x + z * q.w,
                   w * q.w - x * q.x - y * q.y - z * q.z);
  }

  // Quaternion * Vector3 (rotate a point)
  Vector3 operator*(const Vector3 &v) const {
    // quaternion rotation: q * v * q^-1
    Vector3 qvec(x, y, z);
    Vector3 uv = qvec.Cross(v);
    Vector3 uuv = qvec.Cross(uv);

    uv = uv * (2.0f * w);
    uuv = uuv * 2.0f;

    return v + uv + uuv;
  }

  // Rotate a vector using this quaternion
  Vector3 Rotate(const Vector3 &v) const { return (*this) * v; }

  // Rotate a vector using the inverse of this quaternion
  Vector3 RotateInv(const Vector3 &v) const { return conjugate() * v; }

  // Comparison
  bool operator==(const Vector4 &other) const {
    return x == other.x && y == other.y && z == other.z && w == other.w;
  }

  bool operator!=(const Vector4 &other) const { return !(*this == other); }

  // Magnitude
  float Magnitude() const { return sqrtf(x * x + y * y + z * z + w * w); }

  // Normalize
  Vector4 Normalized() const {
    float mag = Magnitude();
    if (mag > 0.00001f) {
      return Vector4(x / mag, y / mag, z / mag, w / mag);
    }
    return Vector4(0, 0, 0, 1);
  }

  void Normalize() {
    float mag = Magnitude();
    if (mag > 0.00001f) {
      x /= mag;
      y /= mag;
      z /= mag;
      w /= mag;
    }
  }

  Vector3 EulerAngles() const {
    float yaw = atan2f(2.f * (w * y + z * x), 1.f - 2.f * (x * x + y * y));
    float pitch = asinf(clamp(2.f * (w * x - y * z), -1.f, 1.f));
    float roll = atan2f(2.f * (w * z + x * y), 1.f - 2.f * (z * z + x * x));
    return {pitch * (180.f / 3.14159265f), yaw * (180.f / 3.14159265f),
            roll * (180.f / 3.14159265f)};
  }

  static Vector4 LookRotation(Vector3 forwardDir,
                              Vector3 up = Vector3(0, 1, 0)) {
    forwardDir = forwardDir.Normalized();
    Vector3 right = up.Cross(forwardDir).Normalized();

    // Handle case where forward and up are parallel/anti-parallel mind bender
    if (right.Length() < 0.0001f) {
      // Forward is parallel to up, use a different right vector still not any
      // better
      if (std::abs(forwardDir.Dot(up)) > 0.9999f) {
        // Forward is nearly parallel to up, use world right fuck me sideways
        right = Vector3(1, 0, 0).Cross(forwardDir).Normalized();
        if (right.Length() < 0.0001f) {
          right = Vector3(0, 0, 1).Cross(forwardDir).Normalized();
        }
      }
    }

    up = forwardDir.Cross(right).Normalized();

    float m00 = right.x;
    float m01 = right.y;
    float m02 = right.z;
    float m10 = up.x;
    float m11 = up.y;
    float m12 = up.z;
    float m20 = forwardDir.x;
    float m21 = forwardDir.y;
    float m22 = forwardDir.z;

    float num8 = (m00 + m11) + m22;
    Vector4 q;
    if (num8 > 0.0f) {
      float num = sqrtf(num8 + 1.0f);
      q.w = num * 0.5f;
      num = 0.5f / num;
      q.x = (m12 - m21) * num;
      q.y = (m20 - m02) * num;
      q.z = (m01 - m10) * num;
      return q;
    }

    // Handle cases when num8 <= 0 (all three cases)
    if ((m00 >= m11) && (m00 >= m22)) {
      float num7 = sqrtf(((1.0f + m00) - m11) - m22);
      float num4 = 0.5f / num7;
      q.x = 0.5f * num7;
      q.y = (m01 + m10) * num4;
      q.z = (m02 + m20) * num4;
      q.w = (m12 - m21) * num4;
      return q;
    }

    if (m11 > m22) {
      float num6 = sqrtf(((1.0f + m11) - m00) - m22);
      float num3 = 0.5f / num6;
      q.x = (m10 + m01) * num3;
      q.y = 0.5f * num6;
      q.z = (m21 + m12) * num3;
      q.w = (m20 - m02) * num3;
      return q;
    }

    float num5 = sqrtf(((1.0f + m22) - m00) - m11);
    float num2 = 0.5f / num5;
    q.x = (m20 + m02) * num2;
    q.y = (m21 + m12) * num2;
    q.z = 0.5f * num5;
    q.w = (m01 - m10) * num2;
    return q;
  }

private:
  float clamp(float n, float lower, float upper) const {
    return fmaxf(lower, fminf(n, upper));
  }
};

struct Matrix4x4 {
  // Row 0
  float _11, _12, _13, _14;
  // Row 1
  float _21, _22, _23, _24;
  // Row 2
  float _31, _32, _33, _34;
  // Row 3
  float _41, _42, _43, _44;
};

namespace Math {
inline float Dot(const Vector3 &a, const Vector3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
} 
