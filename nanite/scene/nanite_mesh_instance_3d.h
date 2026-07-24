/**************************************************************************/
/*  nanite_mesh_instance_3d.h                                             */
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

#include "scene/3d/mesh_instance_3d.h"
#include "core/nanite_resource.h"

class NaniteServer;

// NaniteMeshInstance3D is the user-facing scene node for placing Nanite
// geometry in a 3D scene. It inherits MeshInstance3D so that the engine's
// mesh RID management, frustum culling, shadow casting, and material
// system all work out of the box — the shadow_mesh from NaniteMeshResource
// is assigned to the base mesh, and Nanite's GPU pipeline (when enabled)
// replaces the main render with visibility-buffer rasterization.
class NaniteMeshInstance3D : public MeshInstance3D {
	GDCLASS(NaniteMeshInstance3D, MeshInstance3D);

private:
	Ref<NaniteMeshResource> nanite_mesh;
	RID nanite_mesh_rid; // RID returned by NaniteServer::register_mesh
	bool nanite_enabled = true;
	int forced_lod = -1; // -1 = automatic LOD selection
	float relative_screen_size = 0.0f;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	// Resource binding.
	void set_nanite_mesh(const Ref<NaniteMeshResource> &p_mesh);
	Ref<NaniteMeshResource> get_nanite_mesh() const;

	// Enable/disable Nanite GPU pipeline for this instance.
	void set_nanite_enabled(bool p_enabled);
	bool get_nanite_enabled() const;

	// LOD control.
	void set_forced_lod(int p_lod);
	int get_forced_lod() const;

	void set_relative_screen_size(float p_size);
	float get_relative_screen_size() const;

	// Accessor used by NaniteServer during render callbacks.
	RID get_nanite_mesh_rid() const { return nanite_mesh_rid; }

	NaniteMeshInstance3D();
	~NaniteMeshInstance3D();
};
