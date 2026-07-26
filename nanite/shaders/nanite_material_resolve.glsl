#[compute]

#version 450

#VERSION_DEFINES

// Nanite Stage 1 material-resolve shader (Task 1.16.9).
//
// Resolves the visibility buffer produced by the rasterize pass into a
// shaded color image. For each pixel:
//   1. Read vis_buffer → (cluster_id << 8) | triangle_id
//   2. Read cluster metadata (vertex_offset, vertex_count, triangle_offset,
//      triangle_count, material_index)
//   3. Read 3 local vertex indices from meshlet_triangles (byte offset =
//      triangle_offset + triangle_id * 3)
//   4. Convert local → global vertex indices via meshlet_vertices
//   5. Decode 3 vertices (position + normal + uv) from vertex_data (stride 32 B)
//   6. Transform positions to screen space (MVP), compute barycentric coords
//   7. Interpolate normal + uv via barycentric
//   8. Read material (base_color, metallic, roughness) from materials_ssbo
//   9. Lambert shading: diffuse + ambient
//
// Debug modes (CLUSTER_SOLID_COLOR, LOD_SOLID_COLOR, etc.) are preserved.
// NONE mode now uses real Lambert shading instead of the gray placeholder.
//
// Bindings:
//   0: vis_buffer (r32ui, readonly)
//   1: color_buffer (rgba8, writeonly)
//   2: cluster_ssbo (uint32[], 17 uints per cluster)
//   3: vertex_ssbo (uint32[], raw stride 32 B = 8 uints per vertex)
//   4: materials_ssbo (uint32[], 8 uints per material = 32 B)
//   5: meshlet_vertices_ssbo (uint32[], global vertex indices)
//   6: meshlet_triangles_ssbo (uint32[], packed uint8[] local indices)
//   7: camera_ubo (view_matrix + projection, mat4 each)

layout(set = 0, binding = 0, r32ui) uniform readonly uimage2D vis_buffer;
layout(set = 0, binding = 1, rgba8) uniform writeonly image2D color_buffer;
layout(set = 0, binding = 2) readonly buffer ClusterSSBO {
	uint cluster_data[];
};
layout(set = 0, binding = 3) readonly buffer VertexSSBO {
	uint vertex_data_u32[];
};
layout(set = 0, binding = 4) readonly buffer MaterialsSSBO {
	uint materials_data_u32[];
};
layout(set = 0, binding = 5) readonly buffer MeshletVerticesSSBO {
	uint meshlet_vertices[];
};
layout(set = 0, binding = 6) readonly buffer MeshletTrianglesSSBO {
	uint meshlet_triangles_u32[];
};
layout(set = 0, binding = 7) uniform CameraUniform {
	mat4 view_matrix;
	mat4 projection;
} camera;

layout(push_constant, std430) uniform Params {
	mat4 model_matrix;
	ivec2 screen_size;
	uint debug_mode;
	uint _pad;
} params;

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

const uint CLUSTER_U32_COUNT = 17u;

// --- Cluster field accessors -----------------------------------------
uint cluster_vertex_offset(uint idx)  { return cluster_data[idx * CLUSTER_U32_COUNT + 0u]; }
uint cluster_vertex_count(uint idx)   { return cluster_data[idx * CLUSTER_U32_COUNT + 1u]; }
uint cluster_triangle_offset(uint idx){ return cluster_data[idx * CLUSTER_U32_COUNT + 2u]; }
uint cluster_triangle_count(uint idx) { return cluster_data[idx * CLUSTER_U32_COUNT + 3u]; }
uint cluster_material_index(uint idx) { return cluster_data[idx * CLUSTER_U32_COUNT + 5u]; }

// --- Vertex decode (raw stride 32 B) --------------------------------
vec3 decode_position(uint vertex_index) {
	uint base = vertex_index * 8u;
	return vec3(
		uintBitsToFloat(vertex_data_u32[base + 0u]),
		uintBitsToFloat(vertex_data_u32[base + 1u]),
		uintBitsToFloat(vertex_data_u32[base + 2u])
	);
}
vec3 decode_normal(uint vertex_index) {
	uint base = vertex_index * 8u;
	return vec3(
		uintBitsToFloat(vertex_data_u32[base + 3u]),
		uintBitsToFloat(vertex_data_u32[base + 4u]),
		uintBitsToFloat(vertex_data_u32[base + 5u])
	);
}
vec2 decode_uv(uint vertex_index) {
	uint base = vertex_index * 8u;
	return vec2(
		uintBitsToFloat(vertex_data_u32[base + 6u]),
		uintBitsToFloat(vertex_data_u32[base + 7u])
	);
}

// --- Material decode (32 B per material = 8 uints) -------------------
// Layout: vec4 base_color (r,g,b,a) + vec4 (metallic, roughness, 0, 0)
vec4 decode_material_base_color(uint material_index) {
	uint base = material_index * 8u;
	return vec4(
		uintBitsToFloat(materials_data_u32[base + 0u]),
		uintBitsToFloat(materials_data_u32[base + 1u]),
		uintBitsToFloat(materials_data_u32[base + 2u]),
		uintBitsToFloat(materials_data_u32[base + 3u])
	);
}

// --- Meshlet triangle index decode (packed uint8[]) ------------------
uint read_meshlet_triangle_index(uint byte_offset) {
	uint word_idx = byte_offset >> 2u;
	uint byte_in_word = byte_offset & 3u;
	uint word = meshlet_triangles_u32[word_idx];
	return (word >> (byte_in_word * 8u)) & 0xFFu;
}

// --- Barycentric coordinates (2D, screen space) ----------------------
vec3 barycentric(vec2 p, vec2 a, vec2 b, vec2 c) {
	vec2 v0 = b - a;
	vec2 v1 = c - a;
	vec2 v2 = p - a;
	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;
	if (abs(denom) < 1e-8) {
		return vec3(-1.0);
	}
	float inv = 1.0 / denom;
	float v = (d11 * d20 - d01 * d21) * inv;
	float w = (d00 * d21 - d01 * d20) * inv;
	float u = 1.0 - v - w;
	return vec3(u, v, w);
}

// --- Lambert shading -------------------------------------------------
vec3 lambert(vec3 normal, vec3 light_dir, vec3 base_color) {
	float ndotl = max(dot(normal, light_dir), 0.0);
	float ambient = 0.3;
	return base_color * (ndotl + ambient);
}

// Hash function for cluster id -> color (debug modes).
vec3 hash_color(uint n) {
	float r = float((n * 1664525u) & 255u) / 255.0;
	float g = float((n * 22695477u) & 255u) / 255.0;
	float b = float((n * 34127u) & 255u) / 255.0;
	return vec3(r, g, b);
}

vec3 heat_color(uint depth) {
	float t = float(depth & 7u) / 7.0;
	return mix(vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 0.0), t);
}

void main() {
	ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
	if (pos.x >= params.screen_size.x || pos.y >= params.screen_size.y) {
		return;
	}

	uint encoded = imageLoad(vis_buffer, pos).r;
	if (encoded == 0u) {
		// Empty pixel (no Nanite geometry). Don't write to color_buffer
		// so we don't overwrite engine renderered geometry behind us.
		return;
	}

	uint cluster_id = encoded >> 8u;
	uint triangle_id = encoded & 0xFFu;

	// Read cluster metadata.
	uint v_offset = cluster_vertex_offset(cluster_id);
	uint v_count = cluster_vertex_count(cluster_id);
	uint t_offset = cluster_triangle_offset(cluster_id);
	uint t_count = cluster_triangle_count(cluster_id);
	uint mat_idx = cluster_material_index(cluster_id);

	// Bounds-check triangle_id.
	if (triangle_id >= t_count) {
		return;
	}

	// Read 3 local vertex indices from meshlet_triangles.
	uint li0 = read_meshlet_triangle_index(t_offset + triangle_id * 3u + 0u);
	uint li1 = read_meshlet_triangle_index(t_offset + triangle_id * 3u + 1u);
	uint li2 = read_meshlet_triangle_index(t_offset + triangle_id * 3u + 2u);
	if (li0 >= v_count || li1 >= v_count || li2 >= v_count) {
		return;
	}

	// Convert local → global vertex indices.
	uint gi0 = meshlet_vertices[v_offset + li0];
	uint gi1 = meshlet_vertices[v_offset + li1];
	uint gi2 = meshlet_vertices[v_offset + li2];

	// Decode positions + normals.
	vec3 p0 = decode_position(gi0);
	vec3 p1 = decode_position(gi1);
	vec3 p2 = decode_position(gi2);
	vec3 n0 = decode_normal(gi0);
	vec3 n1 = decode_normal(gi1);
	vec3 n2 = decode_normal(gi2);

	// Transform positions to screen space for barycentric.
	mat4 mvp = camera.projection * camera.view_matrix * params.model_matrix;
	vec4 c0 = mvp * vec4(p0, 1.0);
	vec4 c1 = mvp * vec4(p1, 1.0);
	vec4 c2 = mvp * vec4(p2, 1.0);

	// If any vertex is behind the camera, skip (conservative).
	if (c0.w <= 0.0 || c1.w <= 0.0 || c2.w <= 0.0) {
		return;
	}

	vec2 s0 = (c0.xy / c0.w * 0.5 + 0.5) * vec2(params.screen_size);
	vec2 s1 = (c1.xy / c1.w * 0.5 + 0.5) * vec2(params.screen_size);
	vec2 s2 = (c2.xy / c2.w * 0.5 + 0.5) * vec2(params.screen_size);

	vec2 pixel = vec2(float(pos.x) + 0.5, float(pos.y) + 0.5);
	vec3 bary = barycentric(pixel, s0, s1, s2);
	// The pixel should be inside the triangle (the rasterize pass wrote it),
	// but re-projection may differ slightly. If outside, clamp.
	float bw = max(bary.x, 0.0) + max(bary.y, 0.0) + max(bary.z, 0.0);
	if (bw <= 0.0) {
		return;
	}
	// Normalize so weights sum to 1.
	bary = max(bary, vec3(0.0)) / bw;

	// Interpolate normal (object space → world space via model_matrix).
	vec3 normal_obj = normalize(
		bary.x * n0 + bary.y * n1 + bary.z * n2
	);
	vec3 normal_world = normalize(mat3(params.model_matrix) * normal_obj);

	// Read material base color.
	vec4 base_color = decode_material_base_color(mat_idx);

	vec3 out_color;
	switch (params.debug_mode) {
		case 0u: { // NONE: Lambert shading.
			vec3 light_dir = normalize(vec3(0.5, 0.8, 0.3));
			out_color = lambert(normal_world, light_dir, base_color.rgb);
		} break;
		case 1u: { // CLUSTER_SOLID_COLOR.
			out_color = hash_color(cluster_id);
		} break;
		case 2u: { // LOD_SOLID_COLOR (proxy: triangle_id heat).
			out_color = heat_color(triangle_id);
		} break;
		case 3u: { // OVERDRAW_HEATMAP (Stage 1: same as cluster color).
			out_color = hash_color(cluster_id);
		} break;
		case 4u: { // PAGE_RESIDENCY: all resident in Stage 1, green.
			out_color = vec3(0.0, 1.0, 0.0);
		} break;
		case 5u: { // HZB_MIP_LEVELS: not implemented, magenta.
			out_color = vec3(1.0, 0.0, 1.0);
		} break;
		case 6u: { // HZB_OCCLUSION: not implemented, yellow.
			out_color = vec3(1.0, 1.0, 0.0);
		} break;
		default: {
			out_color = vec3(0.5);
		} break;
	}

	imageStore(color_buffer, pos, vec4(out_color, 1.0));
}
