/**************************************************************************/
/*  nanite_editor_plugin.h                                                */
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

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "./nanite_mesh_editor.h"

#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/mesh.h"

#include "scene/gui/button.h"
#include "scene/gui/box_container.h"

class EditorInspectorPluginNanite : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginNanite, EditorInspectorPlugin);

private:
	// Shared save dialog used by the Convert button for ArrayMesh /
	// MeshInstance3D inputs (Task 0.12.4).
	class EditorFileDialog *convert_save_dialog = nullptr;
	// Stash the object being converted between button click and save confirm.
	ObjectID pending_object_id;
	Ref<class Resource> pending_resource;

	void _on_convert_pressed(Object *p_object);
	void _on_convert_save_confirmed(const String &p_path);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
};

class NaniteEditorPlugin : public EditorPlugin {
	GDCLASS(NaniteEditorPlugin, EditorPlugin);

public:
	NaniteEditorPlugin();
};

#endif // TOOLS_ENABLED
