#include "maths.h"

#include <cmath>



float32 f32_max(float32 a, float32 b)
{
	return a > b ? a : b;
}

float32 f32_clamp(float32 f, float32 min, float32 max)
{
	return f < min ? min : (f > max ? max : f);
}


Vec_3f vec_3f(float32 x, float32 y, float32 z)
{
	Vec_3f v;
	v.x = x;
	v.y = y;
	v.z = z;
	return v;
}

Vec_3f vec_3f_add(Vec_3f a, Vec_3f b)
{
	return vec_3f(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec_3f vec_3f_sub(Vec_3f a, Vec_3f b)
{
	return vec_3f(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec_3f vec_3f_mul(Vec_3f v, float32 f)
{
	return vec_3f(v.x * f, v.y * f, v.z * f);
}

float32 vec_3f_length_sq(Vec_3f v)
{
	return (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
}

Vec_3f vec_3f_normalised(Vec_3f v)
{
	float32 length_sq = (v.x * v.x) + (v.y * v.y) + (v.z * v.z);
	if (length_sq > 0.0f)
	{
		float32 inv_length = 1 / (float32)sqrt(length_sq);
		return vec_3f(v.x * inv_length, v.y * inv_length, v.z * inv_length);
	}

	return v;
}

float32 vec_3f_dot(Vec_3f a, Vec_3f b)
{
	return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

Vec_3f vec_3f_cross(Vec_3f a, Vec_3f b)
{
	return vec_3f((a.y * b.z) - (a.z * b.y), (a.z * b.x) - (a.x * b.z), (a.x * b.y) - (a.y * b.x));
}


Quat quat(float32 zy, float32 xz, float32 yx, float32 scalar)
{
	Quat q;
	q.zy = zy;
	q.xz = xz;
	q.yx = yx;
	q.scalar = scalar;

	return q;
}

Quat quat_identity()
{
	return quat(0.0f, 0.0f, 0.0f, 1.0f);
}

Quat quat_angle_axis(Vec_3f axis, float32 angle)
{
	float32 half_theta = angle * 0.5f;
	float32 sin_half_theta = sinf(half_theta);
	return quat(axis.x * sin_half_theta, axis.y * sin_half_theta, axis.z * sin_half_theta, cosf(half_theta));
}

Quat quat_euler(Vec_3f euler)
{
	Quat pitch = quat_angle_axis(vec_3f(1.0f, 0.0f, 0.0f), euler.x);
	Quat yaw = quat_angle_axis(vec_3f(0.0f, 1.0f, 0.0f), euler.y);
	Quat roll = quat_angle_axis(vec_3f(0.0f, 0.0f, 1.0f), euler.z);
	
	// do roll, then pitch, then yaw
	return quat_mul(yaw, quat_mul(pitch, roll));
}

Vec_3f quat_mul(Quat q, Vec_3f v)
{
	// todo(jbr) simplify

	float32 x = (q.scalar * q.scalar * v.x) + (-2.0f * q.scalar * q.yx * v.y) + (2.0f * q.scalar * -q.xz * v.z) + (-q.zy * -q.zy * v.x) + (2.0f * -q.zy * -q.xz * v.y) + (2.0f * -q.zy * q.yx * v.z) + (-1.0f * -q.xz * -q.xz * v.x) + (-1.0f * q.yx * q.yx * v.x);
	float32 y = (2.0f * q.scalar * q.yx * v.x) + (q.scalar * q.scalar * v.y) + (-2.0f * q.scalar * -q.zy * v.z) + (2.0f * -q.zy * -q.xz * v.x) + (-1.0f * -q.zy * -q.zy * v.y) + (-q.xz * -q.xz * v.y) + (2.0f * -q.xz * q.yx * v.z) + (-1.0f * q.yx * q.yx * v.y);
	float32 z = (-2.0f * q.scalar * -q.xz * v.x) + (2.0f * q.scalar * -q.zy * v.y) + (q.scalar * q.scalar * v.z) + (2.0f * -q.zy * q.yx * v.x) + (-1.0f * -q.zy * -q.zy * v.z) + (2.0f * -q.xz * q.yx * v.y) + (-1.0f * -q.xz * -q.xz * v.z) + (q.yx * q.yx * v.z);
	
	return vec_3f(x, y, z);
}

Quat quat_mul(Quat a, Quat b)
{
	// todo(jbr) simplify

	float32 zy = (b.zy * a.scalar) + (b.scalar * a.zy) + (b.yx * a.xz) + (-1.0f * a.yx * b.xz);
	float32 xz = (b.xz * a.scalar) + (-1.0f * b.yx * a.zy) + (b.scalar * a.xz) + (a.yx * b.zy);
	float32 yx = (b.yx * a.scalar) + (b.xz * a.zy) + (-1.0f * b.zy * a.xz) + (a.yx * b.scalar);
	float32 scalar = (b.scalar * a.scalar) + (-1.0f * b.zy * a.zy) + (-1.0f * b.xz * a.xz) + (-1.0f * a.yx * b.yx);

	return quat(zy, xz, yx, scalar);
}

Vec_3f quat_right(Quat q)
{
	// todo(jbr) on quat simplification, would it be a good idea to cache the square of each component in the struct?
	// we can be faster than quat_mul because we know 2 components of each axis is 0
	return vec_3f(	(q.scalar * q.scalar) + (-q.zy * -q.zy) + (-1.0f * -q.xz * -q.xz) + (-1.0f * q.yx * q.yx),
					(2.0f * q.scalar * q.yx) + (2.0f * -q.zy * -q.xz),
					(-2.0f * q.scalar * -q.xz) + (2.0f * -q.zy * q.yx));
}

Vec_3f quat_forward(Quat q)
{
	return vec_3f(	(-2.0f * q.scalar * q.yx) + (2.0f * -q.zy * -q.xz),
					(q.scalar * q.scalar) + (-1.0f * -q.zy * -q.zy) + (-q.xz * -q.xz) + (-1.0f * q.yx * q.yx),
					(2.0f * q.scalar * -q.zy) + (2.0f * -q.xz * q.yx));
}

Vec_3f quat_up(Quat q)
{
	return vec_3f(	(2.0f * q.scalar * -q.xz) + (2.0f * -q.zy * q.yx),
					(-2.0f * q.scalar * -q.zy) + (2.0f * -q.xz * q.yx),
					(q.scalar * q.scalar) + (-1.0f * -q.zy * -q.zy) + (-1.0f * -q.xz * -q.xz) + (q.yx * q.yx));
}


void matrix_4x4_identity(Matrix_4x4* matrix)
{
	matrix->m11 = 1.0f;
	matrix->m21 = 0.0f;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = 0.0f;
	matrix->m22 = 1.0f;
	matrix->m32 = 0.0f;
	matrix->m42 = 0.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = 0.0f;
	matrix->m33 = 1.0f;
	matrix->m43 = 0.0f;
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = 0.0f;
	matrix->m44 = 1.0f;
}

void matrix_4x4_projection(Matrix_4x4* matrix, 
	float32 fov_y, float32 aspect_ratio,
	float32 near_plane, float32 far_plane)
{
	// create projection matrix
	// Note: Vulkan NDC coordinates are top-left corner (-1, -1), z 0-1
	// 1/(tan(fovx/2)*aspect)	0	0				0
	// 0						0	-1/tan(fovy/2)	0
	// 0						c2	0				c1
	// 0						1	0				0
	// this is stored column major
	// NDC Z = c1/w + c2
	// c1 = (near*far)/(near-far)
	// c2 = far/(far-near)
	matrix->m11 = 1.0f / (tanf(fov_y * 0.5f) * aspect_ratio);
	matrix->m21 = 0.0f;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = 0.0f;
	matrix->m22 = 0.0f;
	matrix->m32 = (far_plane / (far_plane - near_plane));
	matrix->m42 = 1.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = -1.0f / tanf(fov_y * 0.5f);
	matrix->m33 = 0.0f;
	matrix->m43 = 0.0f;
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = (near_plane * far_plane) / (near_plane - far_plane);
	matrix->m44 = 0.0f;
}

void matrix_4x4_translation(Matrix_4x4* matrix, float32 x, float32 y, float32 z)
{
	matrix->m11 = 1.0f;
	matrix->m21 = 0.0f;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = 0.0f;
	matrix->m22 = 1.0f;
	matrix->m32 = 0.0f;
	matrix->m42 = 0.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = 0.0f;
	matrix->m33 = 1.0f;
	matrix->m43 = 0.0f;
	matrix->m14 = x;
	matrix->m24 = y;
	matrix->m34 = z;
	matrix->m44 = 1.0f;
}

void matrix_4x4_translation(Matrix_4x4* matrix, Vec_3f translation)
{
	matrix_4x4_translation(matrix, translation.x, translation.y, translation.z);
}

void matrix_4x4_rotation_x(Matrix_4x4* matrix, float32 r)
{
	float32 cr = cosf(r);
	float32 sr = sinf(r);
	matrix->m11 = 1.0f;
	matrix->m21 = 0.0f;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = 0.0f;
	matrix->m22 = cr;
	matrix->m32 = sr;
	matrix->m42 = 0.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = -sr;
	matrix->m33 = cr;
	matrix->m43 = 0.0f;
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = 0.0f;
	matrix->m44 = 1.0f;
}

void matrix_4x4_rotation_y(Matrix_4x4* matrix, float32 r)
{
	float32 cr = cosf(r);
	float32 sr = sinf(r);
	matrix->m11 = cr;
	matrix->m21 = 0.0f;
	matrix->m31 = -sr;
	matrix->m41 = 0.0f;
	matrix->m12 = 0.0f;
	matrix->m22 = 1.0f;
	matrix->m32 = 0.0f;
	matrix->m42 = 0.0f;
	matrix->m13 = sr;
	matrix->m23 = 0.0f;
	matrix->m33 = cr;
	matrix->m43 = 0.0f;
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = 0.0f;
	matrix->m44 = 1.0f;
}

void matrix_4x4_rotation_z(Matrix_4x4* matrix, float32 r)
{
	float32 cr = cosf(r);
	float32 sr = sinf(r);
	matrix->m11 = cr;
	matrix->m21 = sr;
	matrix->m31 = 0.0f;
	matrix->m41 = 0.0f;
	matrix->m12 = -sr;
	matrix->m22 = cr;
	matrix->m32 = 0.0f;
	matrix->m42 = 0.0f;
	matrix->m13 = 0.0f;
	matrix->m23 = 0.0f;
	matrix->m33 = 1.0f;
	matrix->m43 = 0.0f;
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = 0.0f;
	matrix->m44 = 1.0f;
}

void matrix_4x4_rotation(Matrix_4x4* matrix, Quat rotation)
{
	// x axis
	// we can be faster than quat_mul because we know x is 1, and y/z is 0
	matrix->m11 = (rotation.scalar * rotation.scalar) + (rotation.zy * rotation.zy) + (-1.0f * rotation.xz * rotation.xz) + (-1.0f * rotation.yx * rotation.yx);
	matrix->m21 = (2.0f * rotation.scalar * rotation.yx) + (2.0f * rotation.zy * rotation.xz);
	matrix->m31 = (-2.0f * rotation.scalar * rotation.xz) + (2.0f * rotation.zy * rotation.yx);
	matrix->m41 = 0.0f;

	// y axis
	matrix->m12 = (-2.0f * rotation.scalar * rotation.yx) + (2.0f * rotation.zy * rotation.xz);
	matrix->m22 = (rotation.scalar * rotation.scalar) + (-1.0f * rotation.zy * rotation.zy) + (rotation.xz * rotation.xz) + (-1.0f * rotation.yx * rotation.yx);
	matrix->m32 = (2.0f * rotation.scalar * rotation.zy) + (2.0f * rotation.xz * rotation.yx);
	matrix->m42 = 0.0f;

	// z axis
	matrix->m13 = (2.0f * rotation.scalar * rotation.xz) + (2.0f * rotation.zy * rotation.yx);
	matrix->m23 = (-2.0f * rotation.scalar * rotation.zy) + (2.0f * rotation.xz * rotation.yx);
	matrix->m33 = (rotation.scalar * rotation.scalar) + (-1.0f * rotation.zy * rotation.zy) + (-1.0f * rotation.xz * rotation.xz) + (rotation.yx * rotation.yx);
	matrix->m43 = 0.0f;

	// translation
	matrix->m14 = 0.0f;
	matrix->m24 = 0.0f;
	matrix->m34 = 0.0f;
	matrix->m44 = 1.0f;
}

void matrix_4x4_mul(Matrix_4x4* result, Matrix_4x4* a, Matrix_4x4* b)
{
	assert(result != a && result != b);
	result->m11 = (a->m11 * b->m11) + (a->m12 * b->m21) + (a->m13 * b->m31) + (a->m14 * b->m41);
	result->m21 = (a->m21 * b->m11) + (a->m22 * b->m21) + (a->m23 * b->m31) + (a->m24 * b->m41);
	result->m31 = (a->m31 * b->m11) + (a->m32 * b->m21) + (a->m33 * b->m31) + (a->m34 * b->m41);
	result->m41 = (a->m41 * b->m11) + (a->m42 * b->m21) + (a->m43 * b->m31) + (a->m44 * b->m41);
	result->m12 = (a->m11 * b->m12) + (a->m12 * b->m22) + (a->m13 * b->m32) + (a->m14 * b->m42);
	result->m22 = (a->m21 * b->m12) + (a->m22 * b->m22) + (a->m23 * b->m32) + (a->m24 * b->m42);
	result->m32 = (a->m31 * b->m12) + (a->m32 * b->m22) + (a->m33 * b->m32) + (a->m34 * b->m42);
	result->m42 = (a->m41 * b->m12) + (a->m42 * b->m22) + (a->m43 * b->m32) + (a->m44 * b->m42);
	result->m13 = (a->m11 * b->m13) + (a->m12 * b->m23) + (a->m13 * b->m33) + (a->m14 * b->m43);
	result->m23 = (a->m21 * b->m13) + (a->m22 * b->m23) + (a->m23 * b->m33) + (a->m24 * b->m43);
	result->m33 = (a->m31 * b->m13) + (a->m32 * b->m23) + (a->m33 * b->m33) + (a->m34 * b->m43);
	result->m43 = (a->m41 * b->m13) + (a->m42 * b->m23) + (a->m43 * b->m33) + (a->m44 * b->m43);
	result->m14 = (a->m11 * b->m14) + (a->m12 * b->m24) + (a->m13 * b->m34) + (a->m14 * b->m44);
	result->m24 = (a->m21 * b->m14) + (a->m22 * b->m24) + (a->m23 * b->m34) + (a->m24 * b->m44);
	result->m34 = (a->m31 * b->m14) + (a->m32 * b->m24) + (a->m33 * b->m34) + (a->m34 * b->m44);
	result->m44 = (a->m41 * b->m14) + (a->m42 * b->m24) + (a->m43 * b->m34) + (a->m44 * b->m44);
}

Vec_3f matrix_4x4_mul_direction(Matrix_4x4* matrix, Vec_3f v)
{
	return vec_3f(	(v.x * matrix->m11) + (v.y * matrix->m12) + (v.z * matrix->m13),
					(v.x * matrix->m21) + (v.y * matrix->m22) + (v.z * matrix->m23),
					(v.x * matrix->m31) + (v.y * matrix->m32) + (v.z * matrix->m33));
}

void matrix_4x4_camera(Matrix_4x4* matrix, Vec_3f position, Vec_3f right, Vec_3f forward, Vec_3f up)
{
	// need to use camera position as the effective origin,
	// so negate position to give that translation
	Vec_3f translation = vec_3f_mul(position, -1.0f);

	matrix->m11 = right.x;
	matrix->m21 = forward.x;
	matrix->m31 = up.x;
	matrix->m41 = 0.0f;
	matrix->m12 = right.y;
	matrix->m22 = forward.y;
	matrix->m32 = up.y;
	matrix->m42 = 0.0f;
	matrix->m13 = right.z;
	matrix->m23 = forward.z;	
	matrix->m33 = up.z;
	matrix->m43 = 0.0f;
	matrix->m14 = (right.x * translation.x) + (right.y * translation.y) + (right.z * translation.z);
	matrix->m24 = (forward.x * translation.x) + (forward.y * translation.y) + (forward.z * translation.z);
	matrix->m34 = (up.x * translation.x) + (up.y * translation.y) + (up.z * translation.z);
	matrix->m44 = 1.0f;
}

void matrix_4x4_lookat(Matrix_4x4* matrix, Vec_3f position, Vec_3f target, Vec_3f up)
{
	Vec_3f view_forward = vec_3f_normalised(vec_3f_sub(target, position));
	Vec_3f project_up_onto_forward = vec_3f_mul(view_forward, vec_3f_dot(up, view_forward));
	Vec_3f view_up = vec_3f_normalised(vec_3f_sub(up, project_up_onto_forward));
	Vec_3f view_right = vec_3f_cross(view_forward, view_up);

	matrix_4x4_camera(matrix, position, view_right, view_forward, view_up);
}