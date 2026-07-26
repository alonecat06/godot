/**************************************************************************/
/*  nanite_server.h                                                       */
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

#include "core/object/object.h"
#include "core/object/object_id.h"
#include "nanite/core/nanite_bridge.h" // INaniteBridge
#include "core/templates/hash_map.h"
#include "core/templates/rid.h"
#include "core/templates/rid_owner.h"
#include "nanite/bridge/nanite_gdext_bridge.h" // NaniteGDExtBridge (Ref<> member needs complete type)

class NaniteMeshResource;
class NaniteGPUPipeline;
class NaniteMeshData;
class NaniteMeshInstance3D;
class NanitePageCache;
class NaniteDebug;
class RenderData;
class RenderingDevice;

// NaniteServer is the runtime singleton that owns the mesh registry,
// the instance registry, the page cache and the debug state, and that
// delegates per-frame rendering to the active INaniteBridge + GPU
// pipeline. It is constructed at MODULE_INITIALIZATION_LEVEL_SERVERS
// (see register_types.cpp) and exposed as the "NaniteServer" engine
// singleton so GDScript / C# can reach it via `Engine.get_singleton`.
//
// Stage 1 status:
//   - Mesh registration is ref-counted; on first reference the resource's
//     encoded blobs are uploaded to GPU SSBOs via NaniteMeshData
//     (Task 1.4). Requires a RenderingDevice — in headless mode data
//     stays null and only the RID token is handed back.
//   - render_visibility / render_material_resolve are no-ops until
//     the GPU pipeline (Task 1.5) is wired in.
//   - The bridge is selected via the `nanite/bridge/active` project
//     setting; the gdext backend (Task 1.7) instantiates two
//     NaniteGDExtBridge CompositorEffects in init() when active == "gdext".
//     The effect RIDs are created but not yet attached to the default
//     Compositor (Stage 2 TODO).
class NaniteServer : public Object {
	GDCLASS(NaniteServer, Object);

private:
	static NaniteServer *singleton;

	INaniteBridge *bridge = nullptr;
	INaniteBridge::ShadowMode shadow_mode = INaniteBridge::SHADOW_COARSE_LOD;

	// Stage 1 gdext bridge is split across two CompositorEffect instances
	// (one callback type each): PRE_OPAQUE drives render_visibility,
	// POST_OPAQUE drives render_material_resolve. `bridge` above aliases
	// pre_opaque_bridge.ptr() for the INaniteBridge interface. Both Refs
	// are released in finish(); bridge aliases pre_opaque_bridge.ptr().
	Ref<NaniteGDExtBridge> pre_opaque_bridge;
	Ref<NaniteGDExtBridge> post_opaque_bridge;

	NaniteGPUPipeline *gpu_pipeline = nullptr; // nullptr until Task 1.5.
	NanitePageCache *page_cache = nullptr;
	NaniteDebug *debug = nullptr;

	// mesh_map: resource pointer -> (data, ref_count, rid).
	// data holds the GPU SSBOs once upload_to_gpu has run (requires a
	// RenderingDevice); null in headless mode.
	struct MeshEntry {
		NaniteMeshData *data = nullptr;
		int ref_count = 0;
		RID rid;
	};
	HashMap<const NaniteMeshResource *, MeshEntry> mesh_map;

	// Maps the RID handed back to callers -> the resource pointer.
	// Used by unregister_mesh to find the mesh_map entry from a RID.
	RID_PtrOwner<const NaniteMeshResource> mesh_rid_owner;

	// instance_map: ObjectID -> NaniteMeshInstance3D*.
	HashMap<ObjectID, NaniteMeshInstance3D *> instance_map;

	// Task 1.16.11 — per-instance world transforms, updated each frame by
	// NaniteMeshInstance3D::_notification(NOTIFICATION_TRANSFORM_CHANGED).
	// Keyed by ObjectID to match instance_map.
	HashMap<ObjectID, Transform3D> instance_transforms;

	int visible_cluster_count = 0;

protected:
	static void _bind_methods();

public:
	static NaniteServer *get_singleton() { return singleton; }

	// Lifecycle — called from register_types at MODULE_INITIALIZATION_LEVEL_SERVERS.
	void init();
	void finish();

	// Mesh resource registration (ref-counted).
	RID register_mesh(const Ref<NaniteMeshResource> &p_resource);
	void unregister_mesh(const RID &p_rid);

	// Instance registration.
	void register_instance(NaniteMeshInstance3D *p_instance);
	void unregister_instance(NaniteMeshInstance3D *p_instance);
	int get_instance_count() const { return instance_map.size(); }

	// Task 1.16.11 — per-instance world transform update. Called by
	// NaniteMeshInstance3D::_notification(NOTIFICATION_TRANSFORM_CHANGED).
	// Stores the transform in `instance_transforms` keyed by ObjectID; the
	// next render_visibility / render_material_resolve will pick it up.
	void update_instance_transform(NaniteMeshInstance3D *p_instance, const Transform3D &p_transform);

	// Render callbacks (called by bridge).
	void render_visibility(const RenderData *p_render_data);
	void render_material_resolve(const RenderData *p_render_data);

	// Stats.
	int get_visible_cluster_count() const { return visible_cluster_count; }

	// Debug.
	void set_debug_mode(int p_mode);
	int get_debug_mode() const;

	// Bridge.
	INaniteBridge *get_bridge() const { return bridge; }
	void set_shadow_mode(INaniteBridge::ShadowMode p_mode) { shadow_mode = p_mode; }
	INaniteBridge::ShadowMode get_shadow_mode() const { return shadow_mode; }

	NaniteServer();
	~NaniteServer();
};
