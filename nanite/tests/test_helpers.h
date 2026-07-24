/**************************************************************************/
/*  test_helpers.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/math/math_defs.h"
#include "core/object/ref_counted.h"
#include "core/variant/array.h"
#include "core/variant/variant.h" // PackedInt32Array, PackedVector3Array typedefs
#include "scene/resources/mesh.h"
#include "servers/rendering/rendering_device.h"

#include "../core/nanite_builder.h"
#include "../core/nanite_resource.h"

#include <cstring>

// Test mesh generators for the NaniteBuilder unit tests. All functions
// return a fully-formed ArrayMesh with a single PRIMITIVE_TRIANGLES surface
// (positions + indices only; no normals/UVs needed for the leaf-layer
// pipeline, which only consumes vertex positions).
namespace NaniteTestHelpers {

// Builds a unit cube (corners at ±1) with 24 unique vertices — 4 per face,
// positions duplicated so preprocess_mesh's dedup pass has work to do.
// Produces 12 triangles (6 faces × 2).
inline Ref<ArrayMesh> create_cube_mesh() {
	PackedVector3Array vertices;
	PackedInt32Array indices;

	// Each face is defined by an outward normal and two in-plane tangents
	// (u, v) of unit length. The four corners are normal ± u ± v.
	struct Face {
		Vector3 normal;
		Vector3 u;
		Vector3 v;
	};

	const Face faces[6] = {
		{ Vector3(1, 0, 0), Vector3(0, 0, -1), Vector3(0, 1, 0) }, // +X
		{ Vector3(-1, 0, 0), Vector3(0, 0, 1), Vector3(0, 1, 0) }, // -X
		{ Vector3(0, 1, 0), Vector3(1, 0, 0), Vector3(0, 0, -1) }, // +Y
		{ Vector3(0, -1, 0), Vector3(1, 0, 0), Vector3(0, 0, 1) }, // -Y
		{ Vector3(0, 0, 1), Vector3(1, 0, 0), Vector3(0, 1, 0) }, // +Z
		{ Vector3(0, 0, -1), Vector3(-1, 0, 0), Vector3(0, 1, 0) }, // -Z
	};

	for (int f = 0; f < 6; ++f) {
		const Face &face = faces[f];
		Vector3 center = face.normal; // cube has half-extent 1
		int base = vertices.size();
		vertices.push_back(center - face.u - face.v);
		vertices.push_back(center + face.u - face.v);
		vertices.push_back(center + face.u + face.v);
		vertices.push_back(center - face.u + face.v);
		// Two CCW triangles (winding as viewed from outside the cube).
		indices.push_back(base + 0);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
		indices.push_back(base + 0);
		indices.push_back(base + 2);
		indices.push_back(base + 3);
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

// Builds a UV sphere of unit radius centered at the origin. With
// p_segments = N, produces (N+1)×(N+1) vertices and 2*N*N triangles.
// For p_segments = 32, that's ~2048 triangles (matches the spec's
// "sphere mesh(2000 tri)" test case).
inline Ref<ArrayMesh> create_sphere_mesh(int p_segments = 32) {
	if (p_segments < 4) {
		p_segments = 4;
	}

	PackedVector3Array vertices;
	PackedInt32Array indices;

	const int rings = p_segments; // latitude divisions (pole to pole)
	const int segs = p_segments; // longitude divisions (around equator)

	for (int i = 0; i <= rings; ++i) {
		float theta = Math::PI * (float)i / (float)rings; // 0..π
		float sin_t = Math::sin(theta);
		float cos_t = Math::cos(theta);
		for (int j = 0; j <= segs; ++j) {
			float phi = 2.0f * Math::PI * (float)j / (float)segs; // 0..2π
			float sin_p = Math::sin(phi);
			float cos_p = Math::cos(phi);
			vertices.push_back(Vector3(sin_t * cos_p, cos_t, sin_t * sin_p));
		}
	}

	for (int i = 0; i < rings; ++i) {
		for (int j = 0; j < segs; ++j) {
			int v0 = i * (segs + 1) + j;
			int v1 = v0 + 1;
			int v2 = (i + 1) * (segs + 1) + j;
			int v3 = v2 + 1;
			indices.push_back(v0);
			indices.push_back(v2);
			indices.push_back(v1);
			indices.push_back(v1);
			indices.push_back(v2);
			indices.push_back(v3);
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

// Builds a "large" test mesh with approximately p_target_tris triangles by
// subdividing a planar grid and adding a small sinusoidal Z displacement so
// the resulting geometry has volume (otherwise cluster bounds would be flat
// and would fail the has_volume() test).
inline Ref<ArrayMesh> create_large_test_mesh(int p_target_tris) {
	if (p_target_tris < 2) {
		p_target_tris = 2;
	}

	// N×N quads = 2*N*N triangles, so N = ceil(sqrt(target/2)).
	int n = (int)Math::ceil(Math::sqrt((float)p_target_tris * 0.5f));
	if (n < 1) {
		n = 1;
	}

	PackedVector3Array vertices;
	PackedInt32Array indices;

	for (int i = 0; i <= n; ++i) {
		for (int j = 0; j <= n; ++j) {
			float x = -1.0f + 2.0f * (float)i / (float)n;
			float y = -1.0f + 2.0f * (float)j / (float)n;
			// Small displacement so bounds have volume.
			float z = 0.1f * Math::sin((float)i * 0.7f) * Math::cos((float)j * 0.7f);
			vertices.push_back(Vector3(x, y, z));
		}
	}

	for (int i = 0; i < n; ++i) {
		for (int j = 0; j < n; ++j) {
			int v0 = i * (n + 1) + j;
			int v1 = v0 + 1;
			int v2 = (i + 1) * (n + 1) + j;
			int v3 = v2 + 1;
			indices.push_back(v0);
			indices.push_back(v2);
			indices.push_back(v1);
			indices.push_back(v1);
			indices.push_back(v2);
			indices.push_back(v3);
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_INDEX] = indices;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return mesh;
}

// Creates an R32_SFLOAT 2D texture of size p_width × p_height and fills
// every texel with p_fill_value. Intended for HZB / culling tests that
// need a constant-valued depth source. Returns an empty RID when p_rd is
// null or texture creation fails.
inline RID create_constant_depth_texture(RenderingDevice *p_rd, int p_width, int p_height, float p_fill_value) {
	if (p_rd == nullptr) {
		return RID();
	}

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	tf.width = p_width;
	tf.height = p_height;
	tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;

	RID tex = p_rd->texture_create(tf, RD::TextureView());
	if (!tex.is_valid()) {
		return RID();
	}

	const int pixel_count = p_width * p_height;
	PackedByteArray data;
	data.resize(pixel_count * sizeof(float));
	float *dst = reinterpret_cast<float *>(data.ptrw());
	for (int i = 0; i < pixel_count; ++i) {
		memcpy(dst + i, &p_fill_value, sizeof(float));
	}

	p_rd->texture_update(tex, 0, data);
	return tex;
}

// Specialized 4×4 R32_SFLOAT depth texture builder: fills the texture from
// the 16 floats in p_data (row-major). Handy for HZB tests that need to
// construct specific 2×2 region patterns to verify downsampling behavior.
// Returns an empty RID when p_rd is null or texture creation fails.
inline RID create_depth_texture_4x4(RenderingDevice *p_rd, const float p_data[16]) {
	if (p_rd == nullptr) {
		return RID();
	}

	RD::TextureFormat tf;
	tf.format = RD::DATA_FORMAT_R32_SFLOAT;
	tf.width = 4;
	tf.height = 4;
	tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_CAN_UPDATE_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;

	RID tex = p_rd->texture_create(tf, RD::TextureView());
	if (!tex.is_valid()) {
		return RID();
	}

	PackedByteArray data;
	data.resize(16 * sizeof(float));
	memcpy(data.ptrw(), p_data, 16 * sizeof(float));

	p_rd->texture_update(tex, 0, data);
	return tex;
}

// Builds a NaniteMeshResource from a UV-sphere mesh (segments = p_segments,
// clamped to a minimum of 4). Runs the full NaniteBuilder Stage-0 pipeline.
// Returns a null Ref if the build fails — does not throw.
inline Ref<NaniteMeshResource> build_test_resource_sphere(int p_segments = 32) {
	if (p_segments < 4) {
		p_segments = 4;
	}

	Ref<ArrayMesh> mesh = create_sphere_mesh(p_segments);
	if (mesh.is_null()) {
		return Ref<NaniteMeshResource>();
	}

	Ref<NaniteBuilder> builder;
	builder.instantiate();
	Ref<NaniteMeshResource> resource = builder->build(mesh);

	if (resource.is_null()) {
		return Ref<NaniteMeshResource>();
	}
	return resource;
}

} // namespace NaniteTestHelpers
