/**************************************************************************/
/*  nanite_debug.h                                                        */
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

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/variant/type_info.h"

// NaniteDebug holds the runtime visualization/debug state used by the
// GPU pipeline and the bridge to pick shader permutations or draw
// debug overlays. It is owned by NaniteServer and exposed to the
// singleton so editor tooling can flip modes at runtime.
//
// Stage 0 refactor (2026-07-28): two orthogonal enums were added to split
// the old single DebugMode axis into DisplayMode (what to draw) and
// LODMode (which LOD to draw). The legacy DebugMode enum is kept for the
// Stage 1 GPU pipeline (nanite_material_resolve.glsl still reads it via
// NaniteServer::set_debug_mode); new editor tooling should use the new
// DisplayMode + LODMode pair.
class NaniteDebug : public Object {
	GDCLASS(NaniteDebug, Object);

public:
	// ---- List 1: Display Mode (Stage 0 refactor) -----------------------
	// Controls how the preview viewport shades the mesh. Stage 0 renders
	// these via Godot's standard MeshInstance3D (no Nanite GPU pipeline
	// dependency); Stage 1 may additionally drive the
	// nanite_material_resolve.glsl `display_mode` push constant from the
	// same enum for parity.
	enum DisplayMode {
		NORMAL = 0, // Lambert diffuse (uses shadow_mesh)
		NORMAL_WIREFRAME, // Lambert + white wireframe overlay
		CLUSTER_SOLID, // Per-cluster unique solid color (HSV hash)
		CLUSTER_SOLID_WIREFRAME, // Per-cluster color + white wireframe
		WIREFRAME_ONLY, // Black background, white wireframe only
	};

	// ---- List 2: LOD Mode (Stage 0 refactor) ----------------------------
	// Controls which clusters the preview renders. Stage 0 only implements
	// FORCE_LOD_LEVEL (filter clusters by group_id). NANITE_AUTO is the
	// Stage 1 path (real cull + LOD error selection); selecting it in
	// Stage 0 emits a warning and falls back to FORCE_LOD_LEVEL 0.
	enum LODMode {
		NANITE_AUTO = 0, // Stage 1: auto cull + LOD selection
		FORCE_LOD_LEVEL, // Stage 0: render only clusters with group_id == force_lod_level
	};

	// ---- Legacy DebugMode (Stage 1 GPU pipeline) -----------------------
	// Used by nanite_material_resolve.glsl via NaniteServer::set_debug_mode
	// to switch the per-pixel debug color output. Kept for backward
	// compatibility; new Stage 0 editor tooling should NOT touch this.
	// (Constants kept under their original names so existing tests +
	// Stage 1 shaders continue to compile unchanged.)
	enum DebugMode {
		NONE = 0,
		CLUSTER_SOLID_COLOR,
		LOD_SOLID_COLOR,
		OVERDRAW_HEATMAP,
		PAGE_RESIDENCY,
		HZB_MIP_LEVELS,
		HZB_OCCLUSION,
	};

private:
	// Stage 0 state — two orthogonal axes.
	DisplayMode display_mode = NORMAL;
	LODMode lod_mode = FORCE_LOD_LEVEL; // Stage 0 default
	int force_lod_level = 0; // 0 = finest; n = coarsest (group_id == n)
	bool show_bounds = false;

	// Legacy Stage 1 GPU pipeline debug state.
	DebugMode mode = NONE;
	bool wireframe = false;

protected:
	static void _bind_methods();

public:
	// Stage 0 API.
	void set_display_mode(int p_mode);
	int get_display_mode() const;
	DisplayMode get_display_mode_enum() const { return display_mode; }

	void set_lod_mode(int p_mode);
	int get_lod_mode() const;
	LODMode get_lod_mode_enum() const { return lod_mode; }

	void set_force_lod_level(int p_level);
	int get_force_lod_level() const { return force_lod_level; }

	void set_show_bounds(bool p_show);
	bool get_show_bounds() const { return show_bounds; }

	// Legacy Stage 1 GPU pipeline API (kept for NaniteServer::set_debug_mode).
	void set_mode(int p_mode);
	int get_mode() const;
	DebugMode get_mode_enum() const { return mode; }

	void set_wireframe(bool p_wireframe);
	bool get_wireframe() const { return wireframe; }

	NaniteDebug() = default;
	~NaniteDebug() = default;
};

VARIANT_ENUM_CAST(NaniteDebug::DisplayMode);
VARIANT_ENUM_CAST(NaniteDebug::LODMode);
VARIANT_ENUM_CAST(NaniteDebug::DebugMode);
