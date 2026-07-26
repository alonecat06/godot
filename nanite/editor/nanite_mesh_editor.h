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
#include "scene/gui/label.h"
#include "scene/gui/button.h"
#include "scene/gui/subviewport_container.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "nanite/scene/nanite_mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/mesh.h"

class NaniteMeshResource;

// NaniteMeshEditor is an Inspector-embedded 3D preview widget for
// NaniteMeshResource. It mirrors the structure of MeshEditor (editor/plugins/
// mesh_editor_plugin.cpp): a SubViewportContainer hosting a SubViewport with a
// rotation pivot Node3D, Camera3D, two DirectionalLights, and a MeshInstance3D
// for the shadow mesh. A stats Label overlays cluster/node/page counts +
// shadow triangle count + estimated memory.
//
// All file I/O and GPU pipeline work belongs to later stages; this widget
// only renders the (already-built) shadow_mesh via a standard MeshInstance3D.
class NaniteMeshEditor : public SubViewportContainer {
	GDCLASS(NaniteMeshEditor, SubViewportContainer);

private:
	SubViewport *viewport = nullptr;
	Node3D *rotation_node = nullptr;
	Camera3D *camera = nullptr;
	DirectionalLight3D *light1 = nullptr;
	DirectionalLight3D *light2 = nullptr;
	NaniteMeshInstance3D *mesh_instance = nullptr;
	OptionButton *debug_mode_btn = nullptr;
	Button *wireframe_btn = nullptr;
	Button *bounds_btn = nullptr;
	Label *stats_label = nullptr;

	Ref<NaniteMeshResource> current_resource;

	bool dragging = false;
	float rot_x = 0.0f;
	float rot_y = 0.0f;

	void _update_rotation();
	void _on_debug_mode_changed(int p_index);

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
