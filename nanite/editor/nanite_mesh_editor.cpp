/**************************************************************************/
/*  nanite_mesh_editor.cpp                                                */
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

#ifdef TOOLS_ENABLED

#include "nanite_mesh_editor.h"

#include "nanite/core/nanite_resource.h"
#include "nanite/core/nanite_server.h"
#include "nanite/scene/nanite_mesh_instance_3d.h"

#if defined(NANITE_BRIDGE_GDEXT)
#include "nanite/bridge/nanite_gdext_bridge_manager.h"
#endif

#include "core/math/math_funcs.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/variant/variant.h"
#include "editor/editor_node.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/world_3d.h"

void NaniteMeshEditor::_bind_methods() {
}

void NaniteMeshEditor::_update_rotation() {
	Transform3D t;
	t.basis.rotate(Vector3(0, 1, 0), -rot_y);
	t.basis.rotate(Vector3(1, 0, 0), -rot_x);
	rotation_node->set_transform(t);
}

void NaniteMeshEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_FOCUS_ENTER: {
			// Apply the current OptionButton selection to the NaniteServer
			// when the preview viewport gains focus.
			if (debug_mode_btn) {
				int mode = debug_mode_btn->get_selected();
				// Remap OptionButton index -> NaniteDebug::DebugMode.
				// Index 0 = NONE; indices 1..6 map to CLUSTER_SOLID_COLOR..HZB_OCCLUSION
				// (we skip the legacy "Wireframe" / "Bounds" toggle entries; those are
				//  handled by wireframe_btn / bounds_btn).
				if (NaniteServer::get_singleton() != nullptr) {
					NaniteServer::get_singleton()->set_debug_mode(mode);
				}
			}
		} break;
		case NOTIFICATION_FOCUS_EXIT: {
			// Restore NONE so debug mode doesn't leak to the main editor viewport.
			if (NaniteServer::get_singleton() != nullptr) {
				NaniteServer::get_singleton()->set_debug_mode(0); // NONE
			}
		} break;
		default: {
			// No-op for other notifications.
		} break;
	}
}

void NaniteMeshEditor::_on_debug_mode_changed(int p_index) {
	if (NaniteServer::get_singleton() != nullptr) {
		NaniteServer::get_singleton()->set_debug_mode(p_index);
	}
}

void NaniteMeshEditor::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (mb->get_button_index() == MouseButton::LEFT) {
			if (mb->is_pressed()) {
				dragging = true;
				accept_event();
			} else {
				dragging = false;
				accept_event();
			}
		}
		return;
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && dragging) {
		rot_x -= mm->get_relative().y * 0.01;
		rot_y -= mm->get_relative().x * 0.01;

		rot_x = CLAMP(rot_x, -Math::PI / 2, Math::PI / 2);
		_update_rotation();
		accept_event();
	}
}

void NaniteMeshEditor::edit(const Ref<NaniteMeshResource> &p_resource) {
	current_resource = p_resource;

	if (current_resource.is_null()) {
		mesh_instance->set_nanite_mesh(Ref<NaniteMeshResource>());
		stats_label->set_text("");
		return;
	}

	Ref<ArrayMesh> shadow_mesh = current_resource->get_shadow_mesh();
	mesh_instance->set_nanite_mesh(current_resource);

	// Auto-fit camera distance based on shadow mesh AABB (if present).
	if (shadow_mesh.is_valid()) {
		AABB aabb = shadow_mesh->get_aabb();
		if (aabb.size.length() > 0.0) {
			float distance = aabb.size.length() * 1.2f;
			camera->set_position(Vector3(0, 0, distance));
		}
	}

	// Build stats label text: cluster/node/page counts + shadow triangle
	// count + estimated memory (sum of all blob sizes in MB).
	const int cluster_count = current_resource->get_cluster_count();
	const int node_count = current_resource->get_node_count();
	const int page_count = current_resource->get_page_count();

	int shadow_tri_count = 0;
	if (shadow_mesh.is_valid()) {
		const int surface_count = shadow_mesh->get_surface_count();
		for (int i = 0; i < surface_count; ++i) {
			const int index_count = shadow_mesh->surface_get_array_index_len(i);
			if (index_count > 0) {
				shadow_tri_count += index_count / 3;
			} else {
				const int vert_count = shadow_mesh->surface_get_array_len(i);
				shadow_tri_count += vert_count / 3;
			}
		}
	}

	const uint64_t vertex_bytes = current_resource->get_vertex_data().size();
	const uint64_t clusters_bytes = current_resource->get_clusters_data().size();
	const uint64_t nodes_bytes = current_resource->get_nodes_data().size();
	const uint64_t page_table_bytes = current_resource->get_page_table_data().size();
	const uint64_t total_bytes = vertex_bytes + clusters_bytes + nodes_bytes + page_table_bytes;
	const double total_mb = static_cast<double>(total_bytes) / (1024.0 * 1024.0);

	String stats_text = vformat(
			"Clusters: %d\nNodes: %d\nPages: %d\nShadow Tris: %d\nMemory: %.2f MB",
			cluster_count, node_count, page_count, shadow_tri_count, total_mb);
	stats_label->set_text(stats_text);

	// Reset rotation to a pleasant default orientation.
	rot_x = Math::deg_to_rad(-15.0f);
	rot_y = Math::deg_to_rad(30.0f);
	_update_rotation();
}

NaniteMeshEditor::NaniteMeshEditor() {
	viewport = memnew(SubViewport);
	Ref<World3D> world_3d;
	world_3d.instantiate();
	viewport->set_world_3d(world_3d); // Use own world.
	add_child(viewport);
	viewport->set_disable_input(true);
	viewport->set_msaa_3d(Viewport::MSAA_4X);
	set_stretch(true);

#if defined(NANITE_BRIDGE_GDEXT)
	// Task 1.17.6 — attach the nanite compositor to the preview viewport so
	// PRE_OPAQUE / POST_OPAQUE callbacks fire every frame while the
	// NaniteMeshEditor is open. attach_to_viewport defers the actual
	// World3D wiring until the SubViewport is inside the tree.
	if (NaniteGDExtBridgeManager::get_singleton() != nullptr) {
		NaniteGDExtBridgeManager::get_singleton()->attach_to_viewport(viewport);
	}
#endif

	camera = memnew(Camera3D);
	camera->set_transform(Transform3D(Basis(), Vector3(0, 0, 3)));
	camera->set_perspective(45, 0.1, 100);
	viewport->add_child(camera);

	light1 = memnew(DirectionalLight3D);
	light1->set_transform(Transform3D().looking_at(Vector3(-1, -1, -1), Vector3(0, 1, 0)));
	viewport->add_child(light1);

	light2 = memnew(DirectionalLight3D);
	light2->set_transform(Transform3D().looking_at(Vector3(0, 1, 0), Vector3(0, 0, 1)));
	light2->set_color(Color(0.7, 0.7, 0.7));
	viewport->add_child(light2);

	rotation_node = memnew(Node3D);
	viewport->add_child(rotation_node);

	mesh_instance = memnew(NaniteMeshInstance3D);
	rotation_node->add_child(mesh_instance);

	set_custom_minimum_size(Size2(0, 150) * EDSCALE);

	// Overlay HBoxContainer at the bottom of the SubViewportContainer holding
	// the debug-mode OptionButton, two toggle Buttons, and a spacer.
	HBoxContainer *hb = memnew(HBoxContainer);
	add_child(hb);
	hb->set_anchors_and_offsets_preset(Control::PRESET_BOTTOM_WIDE, Control::PRESET_MODE_MINSIZE, 2);

	debug_mode_btn = memnew(OptionButton);
	debug_mode_btn->add_item("None");                  // 0 = NONE
	debug_mode_btn->add_item("Cluster Solid Color");   // 1 = CLUSTER_SOLID_COLOR
	debug_mode_btn->add_item("LOD Solid Color");       // 2 = LOD_SOLID_COLOR
	debug_mode_btn->add_item("Overdraw Heatmap");      // 3 = OVERDRAW_HEATMAP
	debug_mode_btn->add_item("Page Residency");        // 4 = PAGE_RESIDENCY
	debug_mode_btn->add_item("HZB Mip Levels");        // 5 = HZB_MIP_LEVELS
	debug_mode_btn->add_item("HZB Occlusion");         // 6 = HZB_OCCLUSION
	debug_mode_btn->select(0);
	debug_mode_btn->set_custom_minimum_size(Size2(150, 0) * EDSCALE);
	hb->add_child(debug_mode_btn);

	// Connect item_selected to forward debug mode changes to NaniteServer.
	debug_mode_btn->connect("item_selected", callable_mp(this, &NaniteMeshEditor::_on_debug_mode_changed));

	wireframe_btn = memnew(Button);
	wireframe_btn->set_text("Wireframe");
	wireframe_btn->set_toggle_mode(true);
	hb->add_child(wireframe_btn);

	bounds_btn = memnew(Button);
	bounds_btn->set_text("Bounds");
	bounds_btn->set_toggle_mode(true);
	hb->add_child(bounds_btn);

	hb->add_spacer();

	// Stats label overlay in the top-left of the preview area.
	stats_label = memnew(Label);
	add_child(stats_label);
	stats_label->set_anchors_and_offsets_preset(Control::PRESET_TOP_LEFT, Control::PRESET_MODE_MINSIZE, 4);
	stats_label->set_text("");

	rot_x = 0.0f;
	rot_y = 0.0f;

	EditorNode::get_singleton()->register_hdr_viewport(viewport);
}

NaniteMeshEditor::~NaniteMeshEditor() {
}

#endif // TOOLS_ENABLED
