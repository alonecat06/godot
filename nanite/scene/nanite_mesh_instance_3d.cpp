/**************************************************************************/
/*  nanite_mesh_instance_3d.cpp                                           */
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

#include "scene/nanite_mesh_instance_3d.h"

#include "core/nanite_server.h"
#include "core/nanite_resource.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_server.h"

void NaniteMeshInstance3D::set_nanite_mesh(const Ref<NaniteMeshResource> &p_mesh) {
	// Unregister old resource if any.
	if (nanite_mesh_rid.is_valid()) {
		if (NaniteServer *ns = NaniteServer::get_singleton()) {
			ns->unregister_mesh(nanite_mesh_rid);
		}
		nanite_mesh_rid = RID();
	}

	nanite_mesh = p_mesh;

	if (nanite_mesh.is_valid() && nanite_enabled) {
		// Register with NaniteServer to get GPU mesh RID.
		if (NaniteServer *ns = NaniteServer::get_singleton()) {
			nanite_mesh_rid = ns->register_mesh(nanite_mesh);
		}

		// Set the shadow mesh as the base mesh so the engine renders something
		// (frustum culling, materials, shadows all work via MeshInstance3D).
		Ref<ArrayMesh> shadow = nanite_mesh->get_shadow_mesh();
		if (shadow.is_valid()) {
			set_mesh(shadow);

			// Set the shadow mesh on the rendering server's mesh RID so
			// the engine shadow pass uses the coarse LOD. The mesh RID is
			// exposed by VisualInstance3D as the instance "base".
			RID mesh_rid = get_base();
			if (mesh_rid.is_valid()) {
				RenderingServer::get_singleton()->mesh_set_shadow_mesh(mesh_rid, shadow->get_rid());
			}
		} else {
			WARN_PRINT_ONCE_ED("NaniteMeshInstance3D: resource has no shadow mesh; falling back to engine default rendering.");
		}
	}
}

void NaniteMeshInstance3D::set_nanite_enabled(bool p_enabled) {
	if (nanite_enabled == p_enabled) {
		return;
	}
	nanite_enabled = p_enabled;

	if (!nanite_enabled) {
		// Unregister from server.
		if (nanite_mesh_rid.is_valid()) {
			if (NaniteServer *ns = NaniteServer::get_singleton()) {
				ns->unregister_mesh(nanite_mesh_rid);
			}
			nanite_mesh_rid = RID();
		}
		if (is_inside_tree()) {
			if (NaniteServer *ns = NaniteServer::get_singleton()) {
				ns->unregister_instance(this);
			}
			// Task 1.16.12 — restore default shadow casting so the engine
			// renders the mesh normally (main + shadow passes).
			set_cast_shadows_setting(SHADOW_CASTING_SETTING_ON);
		}
	} else {
		// Re-register.
		if (nanite_mesh.is_valid()) {
			if (NaniteServer *ns = NaniteServer::get_singleton()) {
				nanite_mesh_rid = ns->register_mesh(nanite_mesh);
			}
			// Re-set shadow mesh.
			Ref<ArrayMesh> shadow = nanite_mesh->get_shadow_mesh();
			if (shadow.is_valid()) {
				set_mesh(shadow);
				RID mesh_rid = get_base();
				if (mesh_rid.is_valid()) {
					RenderingServer::get_singleton()->mesh_set_shadow_mesh(mesh_rid, shadow->get_rid());
				}
			}
		}
		if (is_inside_tree()) {
			if (NaniteServer *ns = NaniteServer::get_singleton()) {
				ns->register_instance(this);
				ns->update_instance_transform(this, get_global_transform());
			}
			// Task 1.16.12 — block native mesh rendering.
			set_cast_shadows_setting(SHADOW_CASTING_SETTING_SHADOWS_ONLY);
		}
	}
}

Ref<NaniteMeshResource> NaniteMeshInstance3D::get_nanite_mesh() const {
	return nanite_mesh;
}

bool NaniteMeshInstance3D::get_nanite_enabled() const {
	return nanite_enabled;
}

void NaniteMeshInstance3D::set_forced_lod(int p_lod) {
	forced_lod = p_lod;
}

int NaniteMeshInstance3D::get_forced_lod() const {
	return forced_lod;
}

void NaniteMeshInstance3D::set_relative_screen_size(float p_size) {
	relative_screen_size = p_size;
}

float NaniteMeshInstance3D::get_relative_screen_size() const {
	return relative_screen_size;
}

void NaniteMeshInstance3D::_notification(int p_what) {
	// NOTE: The GDCLASS _notification_forwardv/_notification_backwardv
	// dispatchers (see core/object/object.h) already chain the parent's
	// _notification, so MeshInstance3D's notifications (skeleton path
	// resolution, mesh translation, etc.) are handled automatically. We
	// only handle Nanite-specific side effects here.
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (nanite_enabled && nanite_mesh.is_valid()) {
				if (NaniteServer *ns = NaniteServer::get_singleton()) {
					ns->register_instance(this);
					// Seed the transform cache immediately so the first
					// render_visibility sees the correct model matrix.
					ns->update_instance_transform(this, get_global_transform());
				}
				// Task 1.16.12 — block native mesh rendering. With Nanite
				// enabled, the engine's main pass should NOT draw the shadow
				// mesh we assigned via set_mesh(); only the shadow pass
				// should use it. SHADOW_CASTING_SETTING_SHADOWS_ONLY hides the
				// instance from the main camera while keeping shadow
				// casting intact (so mesh_set_shadow_mesh still works).
				set_cast_shadows_setting(SHADOW_CASTING_SETTING_SHADOWS_ONLY);
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (NaniteServer *ns = NaniteServer::get_singleton()) {
				ns->unregister_instance(this);
			}
			// Restore default shadow casting so the same node can be
			// reused as a plain MeshInstance3D after nanite_enabled=false.
			set_cast_shadows_setting(SHADOW_CASTING_SETTING_ON);
		} break;
		case NOTIFICATION_TRANSFORM_CHANGED: {
			// Task 1.16.11 — push the new world transform to the server.
			// The cached transform is read by render_visibility (cull +
			// rasterize push constant) and render_material_resolve
			// (re-projection for barycentric interpolation).
			if (nanite_enabled && nanite_mesh.is_valid()) {
				if (NaniteServer *ns = NaniteServer::get_singleton()) {
					ns->update_instance_transform(this, get_global_transform());
				}
			}
		} break;
		default:
			break;
	}
}

void NaniteMeshInstance3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_nanite_mesh", "mesh"), &NaniteMeshInstance3D::set_nanite_mesh);
	ClassDB::bind_method(D_METHOD("get_nanite_mesh"), &NaniteMeshInstance3D::get_nanite_mesh);

	ClassDB::bind_method(D_METHOD("set_nanite_enabled", "enabled"), &NaniteMeshInstance3D::set_nanite_enabled);
	ClassDB::bind_method(D_METHOD("get_nanite_enabled"), &NaniteMeshInstance3D::get_nanite_enabled);

	ClassDB::bind_method(D_METHOD("set_forced_lod", "lod"), &NaniteMeshInstance3D::set_forced_lod);
	ClassDB::bind_method(D_METHOD("get_forced_lod"), &NaniteMeshInstance3D::get_forced_lod);

	ClassDB::bind_method(D_METHOD("set_relative_screen_size", "size"), &NaniteMeshInstance3D::set_relative_screen_size);
	ClassDB::bind_method(D_METHOD("get_relative_screen_size"), &NaniteMeshInstance3D::get_relative_screen_size);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "nanite_mesh", PROPERTY_HINT_RESOURCE_TYPE, "NaniteMeshResource"), "set_nanite_mesh", "get_nanite_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "nanite_enabled"), "set_nanite_enabled", "get_nanite_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "forced_lod", PROPERTY_HINT_RANGE, "-1,16"), "set_forced_lod", "get_forced_lod");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "relative_screen_size"), "set_relative_screen_size", "get_relative_screen_size");
}

NaniteMeshInstance3D::NaniteMeshInstance3D() {
}

NaniteMeshInstance3D::~NaniteMeshInstance3D() {
	// Clean up GPU registration if node is destroyed without exiting tree.
	if (nanite_mesh_rid.is_valid()) {
		if (NaniteServer *ns = NaniteServer::get_singleton()) {
			ns->unregister_mesh(nanite_mesh_rid);
		}
	}
}
