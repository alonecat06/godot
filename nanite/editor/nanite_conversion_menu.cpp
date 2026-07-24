/**************************************************************************/
/*  nanite_conversion_menu.cpp                                             */
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

#include "nanite_conversion_menu.h"

#include "../core/nanite_builder.h"
#include "../core/nanite_resource.h"

#include "core/object/callable_mp.h" // callable_mp
#include "editor/editor_node.h"
#include "editor/editor_interface.h"
#include "editor/editor_log.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/file_system/editor_file_system.h" // EditorFileSystem full def for scan().
#include "scene/gui/dialogs.h"
#include "scene/resources/resource_format_text.h"

#include <chrono>

// Supported mesh resource extensions for the "Convert to Nanite..." entry.
static bool _is_supported_mesh_extension(const String &p_path) {
	String ext = p_path.get_extension().to_lower();
	return ext == "gltf" || ext == "glb" || ext == "fbx" || ext == "obj" ||
			ext == "tres" || ext == "res";
}

void NaniteConversionContextMenu::get_options(const Vector<String> &p_paths) {
	// Only show when exactly one supported mesh file is selected.
	if (p_paths.size() != 1) {
		return;
	}
	if (!_is_supported_mesh_extension(p_paths[0])) {
		return;
	}

	// Capture the path for the callback.
	pending_path = p_paths[0];

	// Register the menu item with a callable to _on_convert_callback.
	add_context_menu_item(
			"Convert to Nanite...",
			callable_mp(this, &NaniteConversionContextMenu::_on_convert_callback),
			Ref<Texture2D>());
}

void NaniteConversionContextMenu::_on_convert_callback() {
	if (pending_path.is_empty()) {
		return;
	}

	// Resolve the actual importable path. Godot stores imported assets as
	// .scn/.res internally; ResourceLoader transparently handles the
	// remap when we pass the original .gltf / .fbx path.
	String load_path = pending_path;

	// Load the resource (may be ArrayMesh or PackedScene for gltf/glb/fbx).
	Ref<Resource> res = ResourceLoader::load(load_path);
	if (res.is_null()) {
		EditorNode::get_singleton()->show_warning(
				vformat("Failed to load resource: %s", load_path),
				"Nanite Conversion");
		pending_path = "";
		return;
	}

	// Stash the resource for the save dialog callback.
	pending_resource = res;

	// Create (or reuse) the save dialog. Default output name mirrors the
	// input file with .nanite.tres extension.
	if (!save_dialog) {
		save_dialog = memnew(EditorFileDialog);
		save_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
		save_dialog->add_filter("*.nanite.tres", "Nanite Mesh Resource");
		save_dialog->set_title("Save Nanite Mesh Resource");
		EditorInterface::get_singleton()->get_base_control()->add_child(save_dialog);
		save_dialog->connect("file_selected",
				callable_mp(this, &NaniteConversionContextMenu::_on_save_confirmed));
	}

	String default_name = pending_path.get_file().get_basename() + ".nanite.tres";
	save_dialog->set_current_path(
			pending_path.get_base_dir().path_join(default_name));
	save_dialog->popup_file_dialog();
}

void NaniteConversionContextMenu::_on_save_confirmed(const String &p_path) {
	if (pending_resource.is_null()) {
		EditorNode::get_singleton()->show_warning(
				"Internal error: pending resource was released.",
				"Nanite Conversion");
		pending_path = "";
		return;
	}

	// Run the build with a progress indicator (covers large meshes > 100ms).
	std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

	// EditorProgress is a RAII struct: registers a task in ctor, ends in dtor.
	// 0 steps means indeterminate progress (we don't know how many stages
	// NaniteBuilder will run internally).
	EditorProgress ep("nanite_convert", "Converting to Nanite...", 0, false);

	Ref<NaniteMeshResource> nanite_res =
			NaniteBuilder::build_from_resource(pending_resource);

	(void)ep; // silence unused-warning if any; dtor handles cleanup.

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	int64_t elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	if (nanite_res.is_null()) {
		EditorNode::get_singleton()->show_warning(
				vformat("Nanite build returned null for: %s\n"
						"Check the console for details.",
						pending_path),
				"Nanite Conversion Failed");
		EditorLog *log = EditorNode::get_log();
		if (log) {
			log->add_message(
					vformat("[Nanite] Conversion FAILED: %s",
							pending_path),
					EditorLog::MSG_TYPE_ERROR);
		}
		pending_path = "";
		pending_resource = Ref<Resource>();
		return;
	}

	// Save the produced resource.
	Error err = ResourceSaver::save(nanite_res, p_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(
				vformat("Failed to save Nanite resource: %s\nError code: %d",
						p_path, err),
				"Nanite Conversion Failed");
		pending_path = "";
		pending_resource = Ref<Resource>();
		return;
	}

	// Report to EditorLog.
	EditorLog *log = EditorNode::get_log();
	if (log) {
		log->add_message(
				vformat("[Nanite] Conversion OK: %s\n  clusters: %d  nodes: %d  pages: %d  time: %lld ms\n  saved: %s",
						pending_path,
						nanite_res->get_cluster_count(),
						nanite_res->get_node_count(),
						nanite_res->get_page_count(),
						elapsed_ms,
						p_path),
				EditorLog::MSG_TYPE_STD);
	}

	// Re-scan the filesystem so the new .tres shows up.
	EditorInterface::get_singleton()->get_resource_filesystem()->scan();

	pending_path = "";
	pending_resource = Ref<Resource>();
}

void NaniteConversionContextMenu::_bind_methods() {
	// No GDScript-exposed methods — the plugin is fully driven via
	// get_options() + the callable registered there.
}

#endif // TOOLS_ENABLED
