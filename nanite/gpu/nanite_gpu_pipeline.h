/**************************************************************************/
/*  nanite_gpu_pipeline.h                                                 */
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

#include "core/templates/rid.h"
#include "gpu/nanite_hzb.h"

class RenderingDevice;
class NaniteMeshData;

// NaniteGPUPipeline owns the compute shaders/pipelines that turn a
// NaniteMeshData into a visibility buffer + depth buffer each frame.
//
// Stage 1 frame flow (driven by the caller, typically NaniteServer):
//   1. ensure_screen_buffers(rd, w, h)          — (re)allocate vis/depth/color
//   2. RID vis = dispatch_cull(rd, params, md)  — BVH cull -> visible list
//   3. dispatch_rasterize(rd, vis, count, md)    — soft-raster visible clusters
//   4. dispatch_hzb_build(rd, depth_buffer)      — rebuild occlusion HZB
//   5. dispatch_material_resolve(...)            — Task 1.11: vis -> color
//
// The pipeline is a pure C++ class (no GDCLASS); lifetime is owned by the
// NaniteServer singleton. It embeds a NaniteHZB so the caller doesn't have to
// track it separately.
class NaniteGPUPipeline {
public:
	// Per-frame cull parameters. view_matrix and projection are column-major
	// float[16] (Projection/Transform3D::columns). They are uploaded to a
	// uniform buffer internally because two mat4s (128 B) alone would fill the
	// RenderingDevice push-constant budget (MAX_PUSH_CONSTANT_SIZE == 128).
	// model_matrix (Task 1.16.6) is the per-instance world transform, uploaded
	// via push constant (column-major float[16]).
	struct CullParams {
		float view_matrix[16];
		float projection[16];
		float model_matrix[16]; // Task 1.16.6 — per-instance world transform
		int screen_size[2];
		float error_threshold;
		uint32_t bvh_node_count;
		uint32_t cluster_count;
	};

private:
	RID cull_shader;
	RID cull_pipeline;
	RID rasterize_shader;
	RID rasterize_pipeline;
	RID material_resolve_shader; // For Task 1.11
	RID material_resolve_pipeline; // For Task 1.11

	// Nearest+clamp sampler bound alongside the HZB texture in the cull set.
	RID hzb_sampler;
	// Persistent camera UBO (updated each dispatch_cull via buffer_update).
	RID camera_ubo;
	// 1x1 R32_SFLOAT fallback bound at the HZB texture slot before the first
	// HZB is built, so the cull uniform set is always valid.
	RID dummy_hzb_texture;

	NaniteHZB hzb;
	bool initialized = false;

	// Per-frame screen-space targets (recreated on resize).
	RID vis_buffer; // R32_UINT: encoded (cluster_id << 8 | triangle_id)
	RID depth_buffer; // R32_SFLOAT: per-pixel depth, seeds the HZB rebuild
	RID color_buffer; // RGBA8: shaded output written by material_resolve (Task 1.11)
	int current_width = 0;
	int current_height = 0;

	// Visible-cluster scratch buffers (grown on demand to fit cluster_count).
	RID visible_clusters_buffer; // uint32_t[capacity]
	RID visible_count_buffer; // uint32_t[1]
	uint32_t current_visible_capacity = 0;

	void _free_screen_buffers(RenderingDevice *p_rd);
	void _free_visible_buffers(RenderingDevice *p_rd);

public:
	void init(RenderingDevice *p_rd);
	void cleanup(RenderingDevice *p_rd);

	// Runs the cull pass. Uploads camera matrices to the camera UBO, (re)zeros
	// the visible-count buffer, grows the visible-clusters buffer if needed,
	// builds the cull uniform set from the mesh SSBOs + HZB texture, and
	// dispatches. Returns the visible_clusters_buffer RID (owned by the
	// pipeline; valid until the next dispatch_cull or cleanup).
	RID dispatch_cull(RenderingDevice *p_rd, const CullParams &p_params, const NaniteMeshData *p_mesh_data);

	// Runs the rasterize pass over the visible clusters. p_visible_count is the
	// number of entries written by the preceding dispatch_cull (read back by
	// the caller; the simplified Stage 1 cull emits exactly cluster_count).
	// p_model_matrix (Task 1.16.6) is the per-instance world transform
	// (column-major float[16]); pass nullptr to use identity.
	void dispatch_rasterize(RenderingDevice *p_rd, const RID &p_visible_buffer, uint32_t p_visible_count, const NaniteMeshData *p_mesh_data, const float *p_model_matrix);

	// Rebuilds the HZB pyramid from the rasterize depth output. Delegates to
	// the embedded NaniteHZB.
	void dispatch_hzb_build(RenderingDevice *p_rd, const RID &p_depth_texture);

	// Task 1.11: resolves the vis buffer into a shaded color image.
	// Decodes (cluster_id << 8 | triangle_id) per pixel and writes a
	// debug-mode-specific color (or Stage 1 placeholder gray for NONE) into
	// the internal color_buffer. p_debug_mode is a NaniteDebug::DebugMode
	// value cast to int. If p_vis_buffer is invalid the pipeline's internal
	// vis_buffer is used (caller convenience). p_model_matrix (Task 1.16.6)
	// is the per-instance world transform (column-major float[16]); pass
	// nullptr to use identity (only used by NONE-mode Lambert shading).
	void dispatch_material_resolve(RenderingDevice *p_rd, const RID &p_vis_buffer, const NaniteMeshData *p_mesh_data, int p_debug_mode, const float *p_model_matrix);

	NaniteHZB *get_hzb() { return &hzb; }
	RID get_vis_buffer() const { return vis_buffer; }
	RID get_depth_buffer() const { return depth_buffer; }
	RID get_color_buffer() const { return color_buffer; }
	bool is_initialized() const { return initialized; }

	// (Re)creates vis_buffer + depth_buffer + color_buffer if the size changed.
	// Called every frame before dispatch_cull/rasterize/material_resolve.
	void ensure_screen_buffers(RenderingDevice *p_rd, int p_width, int p_height);
};
