/**************************************************************************/
/*  nanite_mesh_editor.h                                                  */
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

#ifdef TOOLS_ENABLED

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/label.h"
#include "scene/gui/subviewport_container.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/mesh.h"

class NaniteMeshResource;

// NaniteMeshEditor is an Inspector-embedded 3D preview widget for
// NaniteMeshResource. It mirrors the structure of MeshEditor (editor/plugins/
// mesh_editor_plugin.cpp): a SubViewportContainer hosting a SubViewport with a
// rotation pivot Node3D, Camera3D, two DirectionalLights, and a
// MeshInstance3D that renders the decoded mesh.
//
// Stage 0 refactor (2026-07-28): the old single-axis debug OptionButton +
// Wireframe/Bounds toggles have been replaced with two orthogonal dropdowns:
//
//   - Display Mode (6 options): Normal, Normal+Wireframe, Cluster Solid,
//     Cluster Solid+Wireframe, Wireframe Only.
//   - LOD Mode (2 options): Nanite auto cull+LOD (Stage 1 placeholder) /
//     Force LOD Level (with a SpinBox picking which LOD to render).
//
// Stage 0 rendering is INDEPENDENT of the Nanite GPU pipeline (no
// CompositorEffect, no nanite_cull/rasterize/material_resolve shaders). It
// reads the encoded blobs from NaniteMeshResource CPU-side and builds a
// standard ArrayMesh that Godot's MeshInstance3D renders via the regular
// forward pipeline. This keeps the preview confined to the editor module
// and lets it work even when no Nanite bridge is compiled in.
class NaniteMeshEditor : public SubViewportContainer {
	GDCLASS(NaniteMeshEditor, SubViewportContainer);

private:
	SubViewport *viewport = nullptr;
	Node3D *rotation_node = nullptr;
	Camera3D *camera = nullptr;
	DirectionalLight3D *light1 = nullptr;
	DirectionalLight3D *light2 = nullptr;
	// solid_instance renders the shaded surface (Normal or Cluster Solid).
	// wire_instance renders the white wireframe overlay (visible in modes
	// NORMAL_WIREFRAME / CLUSTER_SOLID_WIREFRAME / WIREFRAME_ONLY).
	// partition_border_solid_instance renders the solid partition border
	// lines on the model surface (depth-test enabled). Only shown in
	// CLUSTER_SOLID_WITH_PARTITION_BORDER mode.
	// partition_border_instance renders the dashed partition border overlay
	// for occluded edges (depth-test disabled, stipple shader). Only shown
	// in CLUSTER_SOLID_WITH_PARTITION_BORDER mode.
	// dimmed_instance renders the dimmed (ghosted) clusters when a single
	// cluster is selected in CLUSTER_SOLID modes. It uses a semi-transparent
	// gray material with depth-test disabled so the ghosted geometry doesn't
	// occlude the selected cluster.
	MeshInstance3D *solid_instance = nullptr;
	MeshInstance3D *wire_instance = nullptr;
	MeshInstance3D *partition_border_solid_instance = nullptr;
	MeshInstance3D *partition_border_instance = nullptr;
	MeshInstance3D *dimmed_instance = nullptr;

	// Stage 0 two-axis UI. Two compact dropdowns top-right (DisplayMode +
	// LODMode). When LODMode == FORCE_LOD_LEVEL, a [-] [N] [+] row appears
	// below the LOD dropdown for adjusting the level.
	OptionButton *display_mode_btn = nullptr; // Dropdown: DisplayMode.
	OptionButton *lod_mode_btn = nullptr; // Dropdown: Nanite auto / Force LOD Level.
	Button *lod_minus_button = nullptr; // Decrease force_lod_level.
	Button *lod_plus_button = nullptr; // Increase force_lod_level.
	Label *lod_value_label = nullptr; // Shows current force_lod_level.
	HBoxContainer *lod_buttons_row = nullptr; // Wraps [-] [N] [+], shown only for Force LOD.
	Label *stats_label = nullptr;
	HBoxContainer *ui_bar = nullptr; // Top-right overlay container.

	Ref<NaniteMeshResource> current_resource;

	bool dragging = false;
	bool panning = false; // Middle-button pan drag.
	bool shift_panning = false; // Shift + left-button pan drag.
	bool updating_ui = false; // Guard against recursive signals while programmatically setting UI state.
	float rot_x = 0.0f;
	float rot_y = 0.0f;
	float camera_distance = 3.0f; // Camera distance from rotation_node origin.
	Vector2 pan_offset; // Camera XY offset in rotation_node local space.
	Vector2 click_pos; // Mouse position at press, for distinguishing click vs drag.
	int selected_cluster_index = -1; // -1 = no selection; >= 0 = global cluster index.

	// Current DisplayMode (NaniteDebug::DisplayMode) and LOD mode + level.
	int current_display_mode = 0; // Default NORMAL.
	int current_lod_mode = 1; // Default FORCE_LOD_LEVEL (1). 0 = NANITE_AUTO.
	int force_lod_level = 0; // Active force LOD level when current_lod_mode == FORCE_LOD_LEVEL.
	int max_lod_level = 0; // Max LOD level from current_resource.

	void _update_rotation();
	void _update_camera_transform();
	void _focus_on_model();
	void _on_display_mode_selected(int p_index);
	void _on_lod_mode_selected(int p_index);
	void _on_lod_minus_pressed();
	void _on_lod_plus_pressed();
	void _refresh_lod_buttons(); // Update enabled state + label of LOD adjust row.

	// Rebuild the preview ArrayMesh from current_resource based on the
	// active DisplayMode + LODMode + force_lod_level. Called whenever the
	// user changes any of the three controls, or when edit() loads a new
	// resource. Cheap enough to run synchronously (<1 ms for typical
	// meshes up to ~50 clusters / few thousand triangles).
	void _rebuild_preview();

	// Build and apply the stats_label text. Shows general resource stats
	// always; when a cluster is selected in a Cluster Solid mode, appends
	// that cluster's details (index, triangle/vertex counts, LOD level,
	// error, bounds) and its partition's details (partition_id, sibling
	// cluster count). Called from edit() and _rebuild_preview().
	void _update_stats_label();

	// CPU-side ray-triangle intersection test against the current LOD's
	// decoded clusters. Returns the global cluster index of the first hit,
	// or -1 if no cluster was hit.
	int _ray_pick_cluster(const Vector2 &p_screen_pos);

	// Cycle selection to the previous (p_direction < 0) or next
	// (p_direction > 0) cluster within the same LOD + partition as the
	// currently selected cluster. Wraps around at list boundaries. No-op
	// unless in CLUSTER_SOLID_WITH_PARTITION_BORDER mode with a valid
	// selection and stored partition_ids_data.
	void _cycle_cluster_in_partition(int p_direction);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	NaniteMeshEditor();
	~NaniteMeshEditor();

	void edit(const Ref<NaniteMeshResource> &p_resource);

	virtual void gui_input(const Ref<InputEvent> &p_event) override;
};

#endif // TOOLS_ENABLED
