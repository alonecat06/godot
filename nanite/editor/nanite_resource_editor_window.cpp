/**************************************************************************/
/*  nanite_resource_editor_window.cpp                                     */
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
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifdef TOOLS_ENABLED

#include "nanite_resource_editor_window.h"

#include "nanite_mesh_editor.h"
#include "nanite/core/nanite_resource.h"

#include "scene/gui/dialogs.h"

void NaniteMeshResourceEditorWindow::_bind_methods() {
}

void NaniteMeshResourceEditorWindow::edit(const Ref<NaniteMeshResource> &p_resource) {
	if (p_resource.is_null()) {
		return;
	}

	// Window title: prefer the resource path's file name; fall back to a
	// generic label for in-memory resources (no path assigned yet).
	String window_title = "Nanite Mesh Viewer";
	const String res_path = p_resource->get_path();
	if (!res_path.is_empty()) {
		window_title += " - " + res_path.get_file();
	}
	set_title(window_title);

	// Delegate 3D preview + stats rendering to the embedded viewer.
	viewer->edit(p_resource);

	// Popup centered, clamped to viewport, with a sane default size. The
	// window is reused across edits (hide_on_ok is true by default, so the
	// OK button hides rather than destroying the window).
	popup_centered_clamped(Size2(800, 600));
}

NaniteMeshResourceEditorWindow::NaniteMeshResourceEditorWindow() {
	set_title("Nanite Mesh Viewer");
	set_ok_button_text("Close");
	set_size(Size2(800, 600));
	// Hide (don't free) on OK so the window can be reused by the next edit().
	set_hide_on_ok(true);

	viewer = memnew(NaniteMeshEditor);
	add_child(viewer);
	// Fill the entire dialog content area above the OK button row.
	viewer->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	// Give the viewer room to render a meaningful 3D preview.
	viewer->set_custom_minimum_size(Size2(750, 500));
}

NaniteMeshResourceEditorWindow::~NaniteMeshResourceEditorWindow() {
}

#endif // TOOLS_ENABLED
