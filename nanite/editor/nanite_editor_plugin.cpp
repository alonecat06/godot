/**************************************************************************/
/*  nanite_editor_plugin.cpp                                              */
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

#include "nanite_editor_plugin.h"

#include "../core/nanite_builder.h"
#include "../core/nanite_resource.h"
#include "nanite_conversion_menu.h"
#include "nanite_resource_preview_gen.h"

#include "core/object/class_db.h"
#include "core/object/callable_mp.h" // callable_mp
#include "editor/editor_node.h"
#include "editor/editor_interface.h"
#include "editor/file_system/editor_file_system.h" // EditorFileSystem full def for scan().
#include "editor/gui/editor_file_dialog.h"
#include "editor/editor_log.h"
#include "scene/gui/dialogs.h"
#include "scene/resources/resource_format_text.h"

#include <chrono>

bool EditorInspectorPluginNanite::can_handle(Object *p_object) {
	// Task 0.12.4 — extend can_handle() to also cover ArrayMesh resources
	// and MeshInstance3D nodes so the Convert button appears for them.
	return Object::cast_to<NaniteMeshResource>(p_object) != nullptr ||
			Object::cast_to<ArrayMesh>(p_object) != nullptr ||
			Object::cast_to<MeshInstance3D>(p_object) != nullptr;
}

void EditorInspectorPluginNanite::parse_begin(Object *p_object) {
	// Path 1: NaniteMeshResource — show the existing preview editor.
	if (NaniteMeshResource *res = Object::cast_to<NaniteMeshResource>(p_object)) {
		Ref<NaniteMeshResource> resource(res);
		NaniteMeshEditor *editor = memnew(NaniteMeshEditor);
		editor->edit(resource);
		add_custom_control(editor);
		return;
	}

	// Path 2: ArrayMesh / MeshInstance3D — show a Convert to Nanite button.
	// (Task 0.12.4)
	bool is_array_mesh = (Object::cast_to<ArrayMesh>(p_object) != nullptr);
	bool is_mesh_instance = (Object::cast_to<MeshInstance3D>(p_object) != nullptr);
	if (!is_array_mesh && !is_mesh_instance) {
		return;
	}

	VBoxContainer *vb = memnew(VBoxContainer);
	Button *btn = memnew(Button);
	btn->set_text(TTR("Convert to Nanite..."));
	btn->set_tooltip_text(TTR("Build a NaniteMeshResource from this mesh and save it as a .nanite.tres file."));
	btn->connect("pressed",
			callable_mp(this, &EditorInspectorPluginNanite::_on_convert_pressed).bind(p_object));
	vb->add_child(btn);
	add_custom_control(vb);
}

void EditorInspectorPluginNanite::_on_convert_pressed(Object *p_object) {
	// Extract a Ref<Resource> for the input object.
	// - ArrayMesh: p_object is the resource itself.
	// - MeshInstance3D: extract the .mesh property.
	Ref<Resource> input_res;
	if (ArrayMesh *am = Object::cast_to<ArrayMesh>(p_object)) {
		input_res = Ref<Resource>(am);
	} else if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_object)) {
		input_res = mi->get_mesh();
		if (input_res.is_null()) {
			EditorNode::get_singleton()->show_warning(
					"MeshInstance3D has no mesh assigned.",
					"Nanite Conversion");
			return;
		}
	} else {
		return;
	}

	pending_object_id = p_object->get_instance_id();
	pending_resource = input_res;

	// Lazily create the save dialog (reused across clicks).
	if (!convert_save_dialog) {
		convert_save_dialog = memnew(EditorFileDialog);
		convert_save_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		convert_save_dialog->add_filter("*.nanite.tres", "Nanite Mesh Resource");
		convert_save_dialog->set_title("Save Nanite Mesh Resource");
		EditorInterface::get_singleton()->get_base_control()->add_child(convert_save_dialog);
		convert_save_dialog->connect("file_selected",
				callable_mp(this, &EditorInspectorPluginNanite::_on_convert_save_confirmed));
	}

	// Default output name: <mesh-name>.nanite.tres in res://.
	String default_name = "mesh.nanite.tres";
	if (ArrayMesh *am = Object::cast_to<ArrayMesh>(p_object)) {
		if (!am->get_path().is_empty()) {
			default_name = am->get_path().get_file().get_basename() + ".nanite.tres";
			convert_save_dialog->set_current_path(
					am->get_path().get_base_dir().path_join(default_name));
		} else {
			convert_save_dialog->set_current_path("res://" + default_name);
		}
	} else if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_object)) {
		String node_name = mi->get_name();
		if (node_name.is_empty()) {
			node_name = "mesh";
		}
		convert_save_dialog->set_current_path(
				"res://" + node_name + ".nanite.tres");
	}

	convert_save_dialog->popup_file_dialog();
}

void EditorInspectorPluginNanite::_on_convert_save_confirmed(const String &p_path) {
	if (pending_resource.is_null()) {
		EditorNode::get_singleton()->show_warning(
				"Internal error: pending resource was released.",
				"Nanite Conversion");
		return;
	}

	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	// EditorProgress is a RAII struct (see editor_node.h).
	EditorProgress ep("nanite_convert_inspector", "Converting to Nanite...", 0, false);

	Ref<NaniteMeshResource> nanite_res =
			NaniteBuilder::build_from_resource(pending_resource);

	(void)ep; // dtor closes the progress dialog.

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	if (nanite_res.is_null()) {
		EditorNode::get_singleton()->show_warning(
				"Nanite build returned null.\n"
				"Check the console for details (empty mesh, unsupported surface, etc.).",
				"Nanite Conversion Failed");
		EditorLog *log = EditorNode::get_log();
		if (log) {
			log->add_message("[Nanite] Inspector conversion FAILED",
					EditorLog::MSG_TYPE_ERROR);
		}
		pending_resource = Ref<Resource>();
		return;
	}

	Error err = ResourceSaver::save(nanite_res, p_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(
				vformat("Failed to save: %s\nError code: %d", p_path, err),
				"Nanite Conversion Failed");
		pending_resource = Ref<Resource>();
		return;
	}

	EditorLog *log = EditorNode::get_log();
	if (log) {
		log->add_message(
				vformat("[Nanite] Inspector conversion OK\n  clusters: %d  nodes: %d  pages: %d  time: %lld ms\n  saved: %s",
						nanite_res->get_cluster_count(),
						nanite_res->get_node_count(),
						nanite_res->get_page_count(),
						elapsed_ms,
						p_path),
				EditorLog::MSG_TYPE_STD);
	}

	EditorInterface::get_singleton()->get_resource_filesystem()->scan();
	pending_resource = Ref<Resource>();
}

NaniteEditorPlugin::NaniteEditorPlugin() {
	Ref<EditorInspectorPluginNanite> plugin;
	plugin.instantiate();
	add_inspector_plugin(plugin);

	// Register the Nanite-specific thumbnail generator so the FileSystem
	// dock can render previews for .nanite / .tres resources carrying a
	// NaniteMeshResource. Mirrors the pattern in CurveEditorPlugin's
	// constructor (editor/scene/curve_editor_plugin.cpp).
	EditorInterface::get_singleton()->get_resource_previewer()->add_preview_generator(memnew(NaniteResourcePreviewGenerator));

	// Task 0.12.3 — register the FileSystem context menu plugin so the
	// user can right-click .gltf / .glb / .fbx / .obj / mesh .tres files
	// and pick "Convert to Nanite...".
	Ref<NaniteConversionContextMenu> menu;
	menu.instantiate();
	add_context_menu_plugin(EditorContextMenuPlugin::CONTEXT_SLOT_FILESYSTEM, menu);
}

#endif // TOOLS_ENABLED
