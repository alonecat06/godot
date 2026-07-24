/**************************************************************************/
/*  nanite_conversion_menu.h                                               */
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

#include "core/object/ref_counted.h"

#include "editor/inspector/editor_context_menu_plugin.h"

// NaniteConversionContextMenu (Task 0.12.3)
//
// Adds a "Convert to Nanite..." entry to the FileSystem dock context menu
// for .gltf / .glb / .fbx / .obj / .tres mesh resources. Clicking it opens
// a save dialog, then calls NaniteBuilder::build_from_resource() and
// ResourceSaver::save() to produce a .nanite.tres file alongside the
// source asset.
//
// Stage 0 deliberately does NOT auto-import on re-import — conversion is
// a manual one-shot action triggered by the user (see design doc 9.7.1
// for rationale).
class NaniteConversionContextMenu : public EditorContextMenuPlugin {
	GDCLASS(NaniteConversionContextMenu, EditorContextMenuPlugin);

private:
	// Path of the file being converted (captured from _popup_menu and
	// consumed by _on_convert_callback). Empty when no conversion is in
	// progress.
	String pending_path;

	// Save dialog (created lazily on first use, reused across invocations).
	class EditorFileDialog *save_dialog = nullptr;

	// The resource being converted (kept alive between dialog open and
	// save confirmation).
	Ref<class Resource> pending_resource;

	void _on_convert_callback(const Variant &p_arg);
	void _on_save_confirmed(const String &p_path);

protected:
	static void _bind_methods();

public:
	virtual void get_options(const Vector<String> &p_paths) override;
};

#endif // TOOLS_ENABLED
