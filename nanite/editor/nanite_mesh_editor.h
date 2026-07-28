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
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
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
//   - Display Mode (5 options): Normal, Normal+Wireframe, Cluster Solid,
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
	MeshInstance3D *solid_instance = nullptr;
	MeshInstance3D *wire_instance = nullptr;

	// Stage 0 two-axis UI.
	OptionButton *display_mode_btn = nullptr; // List 1: DisplayMode
	OptionButton *lod_mode_btn = nullptr; // List 2: LODMode
	SpinBox *force_lod_spinner = nullptr; // List 2 child: force_lod_level
	Label *stats_label = nullptr;

	Ref<NaniteMeshResource> current_resource;

	bool dragging = false;
	bool lod_mode_updating = false; // Guard against recursive item_selected signals.
	float rot_x = 0.0f;
	float rot_y = 0.0f;

	void _update_rotation();
	void _on_display_mode_selected(int p_index);
	void _on_lod_mode_selected(int p_index);
	void _on_force_lod_changed(double p_value);

	// Rebuild the preview ArrayMesh from current_resource based on the
	// active DisplayMode + LODMode + force_lod_level. Called whenever the
	// user changes any of the three controls, or when edit() loads a new
	// resource. Cheap enough to run synchronously (<1 ms for typical
	// meshes up to ~50 clusters / few thousand triangles).
	void _rebuild_preview();

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
