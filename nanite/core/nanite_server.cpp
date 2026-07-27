/**************************************************************************/
/*  nanite_server.cpp                                                      */
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

#include "nanite/core/nanite_server.h"

#include "nanite/core/nanite_debug.h"
#include "nanite/core/nanite_page_cache.h"
#include "nanite/core/nanite_resource.h"
#include "nanite/gpu/nanite_gpu_pipeline.h"
#include "nanite/gpu/nanite_mesh_data.h"
#include "nanite/scene/nanite_mesh_instance_3d.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/math/projection.h"
#include "core/math/transform_3d.h"
#include "core/object/class_db.h"
#include "core/string/string_name.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/storage/render_data.h"
#include "servers/rendering/storage/render_scene_data.h"

// Task 1.17.2 — core must NOT include nanite/bridge/nanite_gdext_bridge.h
// (or any concrete bridge header). The bridge is injected via set_bridge()
// from the bridge layer's Manager. Only abstract INaniteBridge (already
// pulled in via nanite_server.h) is visible here.

// Static singleton pointer; set in init(), cleared in finish().
NaniteServer *NaniteServer::singleton = nullptr;

NaniteServer::NaniteServer() {
}

NaniteServer::~NaniteServer() {
	// Defensive cleanup if finish() was not called explicitly.
	if (singleton == this) {
		singleton = nullptr;
	}
}

void NaniteServer::init() {
	singleton = this;

	// Register the project setting that selects which bridge backend is
	// active, and read its current value in one call. GLOBAL_DEF is
	// idempotent and returns the registered value (existing or default);
	// using its return value avoids a separate GLOBAL_GET, which can trip
	// on ProjectSettings / OS initialization ordering at SERVERS level.
	page_cache = memnew(NanitePageCache);
	debug = memnew(NaniteDebug);

	// Task 1.17.2 — core init no longer creates any concrete bridge.
	// `bridge` stays nullptr here and is injected by the bridge layer's
	// Manager (see register_types.cpp at MODULE_INITIALIZATION_LEVEL_SERVERS)
	// via NaniteServer::set_bridge(). The active backend is selected by
	// which Manager the bridge layer decides to construct; the
	// `nanite/bridge/active` setting is informational only at this level.
	String active_bridge = GLOBAL_DEF(PropertyInfo(Variant::STRING, "nanite/bridge/active", PROPERTY_HINT_ENUM, "gdext,module,deep"), "gdext");
	if (!(active_bridge == "gdext" || active_bridge == "module" || active_bridge == "deep")) {
		ERR_PRINT(vformat("NaniteServer: unknown bridge '%s'. Falling back to no bridge.", active_bridge));
	}

	// Task 1.5 / 1.16.11 — GPU pipeline creation is deferred to
	// set_bridge(). At SERVERS init() time RenderingDevice is not yet
	// available (RenderingServer is created AFTER initialize_modules(SERVERS)
	// in main.cpp setup2()), so creating the pipeline here would always
	// hit the RD-null branch and leave gpu_pipeline perpetually nullptr —
	// causing render_visibility to early-out every frame. set_bridge() is
	// called by the bridge layer's Manager at SCENE level, where RS/RD
	// are guaranteed ready.
}

void NaniteServer::set_bridge(INaniteBridge *p_bridge) {
	bridge = p_bridge;
	// Lazily create + init the GPU pipeline on first bridge injection.
	// At SERVERS init() time RenderingDevice is not yet available, so
	// we can't create it there. By the time a bridge is injected (SCENE
	// level, via NaniteGDExtBridgeManager::init), RS/RD are ready.
	// Also re-init if bridge is being re-set after a previous clear
	// (gpu_pipeline may already exist — don't leak).
	if (p_bridge != nullptr && gpu_pipeline == nullptr) {
		RenderingDevice *rd = RenderingDevice::get_singleton();
		if (rd) {
			gpu_pipeline = memnew(NaniteGPUPipeline);
			gpu_pipeline->init(rd);
		}
	}
}

void NaniteServer::finish() {
	if (page_cache) {
		memdelete(page_cache);
		page_cache = nullptr;
	}
	if (debug) {
		memdelete(debug);
		debug = nullptr;
	}
	if (gpu_pipeline) {
		RenderingDevice *rd = RenderingDevice::get_singleton();
		if (rd) {
			gpu_pipeline->cleanup(rd);
		}
		memdelete(gpu_pipeline);
		gpu_pipeline = nullptr;
	}
	// Task 1.17.2 — bridge is owned by the bridge layer's Manager
	// (NaniteGDExtBridgeManager), not by NaniteServer. Just drop the raw
	// pointer; the Manager clears it via set_bridge(nullptr) before
	// releasing its Refs.
	bridge = nullptr;

	// Free any leaked RIDs / mesh entries (should be empty if callers
	// behaved). Free NaniteMeshData GPU resources + the owning object for
	// any entry still holding them, then drop the RID owner + mesh_map.
	RenderingDevice *rd = RenderingDevice::get_singleton();
	for (const KeyValue<const NaniteMeshResource *, MeshEntry> &E : mesh_map) {
		if (E.value.data) {
			E.value.data->free_gpu_resources(rd);
			memdelete(E.value.data);
		}
		if (E.value.rid.is_valid()) {
			mesh_rid_owner.free(E.value.rid);
		}
	}
	mesh_map.clear();
	instance_map.clear();

	if (singleton == this) {
		singleton = nullptr;
	}
}

RID NaniteServer::register_mesh(const Ref<NaniteMeshResource> &p_resource) {
	ERR_FAIL_COND_V(p_resource.is_null(), RID());

	const NaniteMeshResource *res = p_resource.ptr();

	HashMap<const NaniteMeshResource *, MeshEntry>::Iterator it = mesh_map.find(res);
	if (it) {
		it->value.ref_count++;
		return it->value.rid;
	}

	// New registration. Upload the resource's encoded blobs to GPU SSBOs
	// via NaniteMeshData. RenderingDevice may be null in headless/test
	// mode — in that case data stays null and no GPU upload happens; we
	// still hand back a valid RID token so callers can pair register/unregister.
	MeshEntry entry;
	entry.ref_count = 1;
	entry.rid = mesh_rid_owner.make_rid(res);
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (rd) {
		entry.data = memnew(NaniteMeshData);
		entry.data->upload_to_gpu(rd, res);
	}

	mesh_map.insert(res, entry);
	return entry.rid;
}

void NaniteServer::unregister_mesh(const RID &p_rid) {
	if (p_rid.is_null()) {
		return;
	}

	const NaniteMeshResource *res = mesh_rid_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(res);

	HashMap<const NaniteMeshResource *, MeshEntry>::Iterator it = mesh_map.find(res);
	if (!it) {
		// RID was valid for the owner but not in mesh_map — defensive guard.
		ERR_FAIL_MSG("NaniteServer::unregister_mesh: RID not registered.");
	}

	it->value.ref_count--;
	if (it->value.ref_count <= 0) {
		if (it->value.data) {
			it->value.data->free_gpu_resources(RenderingDevice::get_singleton());
			memdelete(it->value.data);
			it->value.data = nullptr;
		}
		mesh_rid_owner.free(p_rid);
		mesh_map.remove(it);
	}
}

void NaniteServer::register_instance(NaniteMeshInstance3D *p_instance) {
	ERR_FAIL_NULL(p_instance);
	// NaniteMeshInstance3D is forward-declared, so cast to its Object
	// base to reach get_instance_id(). This is safe because every
	// NaniteMeshInstance3D (Task 1.8) will derive from Object.
	ObjectID oid = ((Object *)p_instance)->get_instance_id();
	instance_map[oid] = p_instance;
	// Seed the transform cache with identity so render_visibility never
	// reads an uninitialized entry before the first TRANSFORM_CHANGED.
	instance_transforms[oid] = Transform3D();
}

void NaniteServer::unregister_instance(NaniteMeshInstance3D *p_instance) {
	ERR_FAIL_NULL(p_instance);
	ObjectID oid = ((Object *)p_instance)->get_instance_id();
	instance_map.erase(oid);
	instance_transforms.erase(oid);
}

void NaniteServer::update_instance_transform(NaniteMeshInstance3D *p_instance, const Transform3D &p_transform) {
	ERR_FAIL_NULL(p_instance);
	// Only cache the transform if the instance is currently registered —
	// avoids filling the map with entries for nodes that aren't in tree.
	ObjectID oid = ((Object *)p_instance)->get_instance_id();
	if (instance_map.has(oid)) {
		instance_transforms[oid] = p_transform;
	}
}

void NaniteServer::render_visibility(const RenderData *p_render_data) {
	if (!gpu_pipeline || instance_map.size() == 0) {
		return;
	}
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd || !gpu_pipeline->is_initialized()) {
		return;
	}

	// Task 1.17.7 — read the real camera matrices + render-target size
	// from RenderData instead of using identity / fixed 1280×720. If the
	// RenderData is null (e.g. test path) we fall back to identity +
	// 1280×720 so the dispatches don't crash.
	float view_matrix[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	float projection[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	int screen_w = 1280;
	int screen_h = 720;

	if (p_render_data != nullptr) {
		RenderSceneData *rsd = p_render_data->get_render_scene_data();
		if (rsd != nullptr) {
			// Godot's Projection stores columns[4] (each a Vector4). Column
			// i, row j -> projection[col*4 + row].
			const Projection proj = rsd->get_cam_projection();
			for (int col = 0; col < 4; ++col) {
				for (int row = 0; row < 4; ++row) {
					projection[col * 4 + row] = proj.columns[col][row];
				}
			}
			// View matrix from the camera's world transform: we want
			// view = inverse(world). For a Transform3D `t`, the inverse is
			// (basis.inverse, basis.inverse * -origin), but Godot's Basis
			// is orthonormal for cameras so we use Basis::transposed() to
			// invert the rotation and recompute the translation.
			const Transform3D cam_xf = rsd->get_cam_transform();
			const Basis inv_basis = cam_xf.basis.transposed();
			const Vector3 inv_origin = inv_basis.xform(-cam_xf.origin);
			// Column-major float[16] from the inverse transform.
			view_matrix[0] = inv_basis.rows[0].x; view_matrix[1] = inv_basis.rows[0].y; view_matrix[2] = inv_basis.rows[0].z; view_matrix[3] = 0.0f;
			view_matrix[4] = inv_basis.rows[1].x; view_matrix[5] = inv_basis.rows[1].y; view_matrix[6] = inv_basis.rows[1].z; view_matrix[7] = 0.0f;
			view_matrix[8] = inv_basis.rows[2].x; view_matrix[9] = inv_basis.rows[2].y; view_matrix[10] = inv_basis.rows[2].z; view_matrix[11] = 0.0f;
			view_matrix[12] = inv_origin.x; view_matrix[13] = inv_origin.y; view_matrix[14] = inv_origin.z; view_matrix[15] = 1.0f;
		}

		// Render-target size from the render-scene buffers. The abstract
		// RenderSceneBuffers base class doesn't expose get_internal_size
		// (it's defined on the concrete RenderSceneBuffersRD subclass), so
		// dispatch via Variant. Returns a Vector2i for the engine's
		// internal-size accessor.
		Ref<RenderSceneBuffers> buffers = p_render_data->get_render_scene_buffers();
		if (buffers.is_valid()) {
			const Variant szv = buffers->call(SNAME("get_internal_size"));
			if (szv.get_type() == Variant::VECTOR2I) {
				const Vector2i sz = szv;
				if (sz.width > 0 && sz.height > 0) {
					screen_w = sz.width;
					screen_h = sz.height;
				}
			}
		}
	}

	gpu_pipeline->ensure_screen_buffers(rd, screen_w, screen_h);

	// Reset accumulated visible-cluster count. Each instance's dispatch
	// returns the cull's visible_count, which is added to this counter.
	visible_cluster_count = 0;

	// Task 1.16.11 — iterate every registered instance. Each instance
	// independently drives dispatch_cull + dispatch_rasterize with its own
	// model_matrix. Multiple instances referencing the same NaniteMeshResource
	// share a single NaniteMeshData (ref-counted in mesh_map) — we just look
	// it up via the resource pointer rather than re-uploading.
	bool any_rasterized = false;
	for (const KeyValue<ObjectID, NaniteMeshInstance3D *> &E : instance_map) {
		NaniteMeshInstance3D *inst = E.value;
		if (!inst) {
			continue;
		}
		// Resolve the instance's mesh RID → resource → mesh_map entry.
		RID mesh_rid = inst->get_nanite_mesh_rid();
		if (mesh_rid.is_null()) {
			continue;
		}
		const NaniteMeshResource *res = mesh_rid_owner.get_or_null(mesh_rid);
		if (!res) {
			continue;
		}
		HashMap<const NaniteMeshResource *, MeshEntry>::Iterator mit = mesh_map.find(res);
		if (!mit || !mit->value.data || !mit->value.data->is_gpu_uploaded()) {
			continue;
		}
		NaniteMeshData *md = mit->value.data;

		const int cluster_count = res->get_cluster_count();
		if (cluster_count == 0) {
			continue;
		}

		// Per-instance transform: column-major float[16] from Transform3D.
		// Godot's Basis stores rows[3] where each row is a local axis vector
		// (rows[0] = X axis, rows[1] = Y axis, rows[2] = Z axis expressed in
		// world coords). For a column-major GLSL mat4 each axis becomes a
		// column, so model_matrix[col*4 + row] = basis.rows[col][row].
		// Translation lives in Transform3D::origin → model_matrix[12..14].
		Transform3D tr = instance_transforms.has(E.key) ? instance_transforms[E.key] : Transform3D();
		float model_matrix[16];
		const Basis &b = tr.basis;
		const Vector3 &o = tr.origin;
		// Column-major (matches GLSL mat4 layout). Columns 0..2 = basis
		// columns, column 3 = translation.
		model_matrix[0] = b.rows[0].x; model_matrix[1] = b.rows[0].y; model_matrix[2] = b.rows[0].z; model_matrix[3] = 0.0f;
		model_matrix[4] = b.rows[1].x; model_matrix[5] = b.rows[1].y; model_matrix[6] = b.rows[1].z; model_matrix[7] = 0.0f;
		model_matrix[8] = b.rows[2].x; model_matrix[9] = b.rows[2].y; model_matrix[10] = b.rows[2].z; model_matrix[11] = 0.0f;
		model_matrix[12] = o.x; model_matrix[13] = o.y; model_matrix[14] = o.z; model_matrix[15] = 1.0f;

		NaniteGPUPipeline::CullParams params;
		memcpy(params.view_matrix, view_matrix, sizeof(view_matrix));
		memcpy(params.projection, projection, sizeof(projection));
		memcpy(params.model_matrix, model_matrix, sizeof(model_matrix));
		params.screen_size[0] = screen_w;
		params.screen_size[1] = screen_h;
		params.error_threshold = 0.01f;
		params.bvh_node_count = 1; // Stage 1: BVH traversal not yet implemented (per-cluster parallel cull)
		params.cluster_count = cluster_count;

		RID visible_buffer = gpu_pipeline->dispatch_cull(rd, params, md);
		if (!visible_buffer.is_valid()) {
			continue;
		}
		// Stage 1 simplification: dispatch rasterize with cluster_count as
		// the visible_count upper bound. The rasterize shader's push
		// constant `visible_count` is used for the early-out bounds check
		// (`if (tid >= params.visible_count) return;`), so threads beyond
		// the actual visible list return early without reading uninitialized
		// memory. Stage 2 will read back the real visible_count via
		// buffer_get_data(visible_count_buffer) to avoid wasted threads.
		gpu_pipeline->dispatch_rasterize(rd, visible_buffer, cluster_count, md, model_matrix);
		any_rasterized = true;
		visible_cluster_count += cluster_count;
	}

	if (any_rasterized) {
		gpu_pipeline->dispatch_hzb_build(rd, gpu_pipeline->get_depth_buffer());
	}
}

void NaniteServer::render_material_resolve(const RenderData *p_render_data) {
	(void)p_render_data; // Stage 1 doesn't blend into the engine color target yet.
	if (!gpu_pipeline || instance_map.size() == 0) {
		return;
	}
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd || !gpu_pipeline->is_initialized()) {
		return;
	}
	int debug_mode = debug ? (int)debug->get_mode_enum() : 0;

	// Task 1.16.11 — iterate instances. Material resolve re-projects each
	// instance's triangles via the model_matrix push constant to compute
	// barycentric coordinates in screen space. The vis_buffer is shared
	// (one image per screen), so we resolve the first instance for now;
	// proper multi-instance composition (per-instance vis layers) is a
	// Stage 2 concern. Stage 1 still drives dispatch_material_resolve per
	// instance so each instance's model_matrix is applied.
	for (const KeyValue<ObjectID, NaniteMeshInstance3D *> &E : instance_map) {
		NaniteMeshInstance3D *inst = E.value;
		if (!inst) {
			continue;
		}
		RID mesh_rid = inst->get_nanite_mesh_rid();
		if (mesh_rid.is_null()) {
			continue;
		}
		const NaniteMeshResource *res = mesh_rid_owner.get_or_null(mesh_rid);
		if (!res) {
			continue;
		}
		HashMap<const NaniteMeshResource *, MeshEntry>::Iterator mit = mesh_map.find(res);
		if (!mit || !mit->value.data || !mit->value.data->is_gpu_uploaded()) {
			continue;
		}
		NaniteMeshData *md = mit->value.data;

		Transform3D tr = instance_transforms.has(E.key) ? instance_transforms[E.key] : Transform3D();
		float model_matrix[16];
		const Basis &b = tr.basis;
		const Vector3 &o = tr.origin;
		model_matrix[0] = b.rows[0].x; model_matrix[1] = b.rows[0].y; model_matrix[2] = b.rows[0].z; model_matrix[3] = 0.0f;
		model_matrix[4] = b.rows[1].x; model_matrix[5] = b.rows[1].y; model_matrix[6] = b.rows[1].z; model_matrix[7] = 0.0f;
		model_matrix[8] = b.rows[2].x; model_matrix[9] = b.rows[2].y; model_matrix[10] = b.rows[2].z; model_matrix[11] = 0.0f;
		model_matrix[12] = o.x; model_matrix[13] = o.y; model_matrix[14] = o.z; model_matrix[15] = 1.0f;

		gpu_pipeline->dispatch_material_resolve(rd, gpu_pipeline->get_vis_buffer(), md, debug_mode, model_matrix);
		// Stage 1 single-pass: one dispatch per instance is enough since
		// vis_buffer is shared. Break after the first so we don't overwrite
		// the color_buffer with a second instance's model_matrix.
		break;
	}
}

void NaniteServer::set_debug_mode(int p_mode) {
	if (debug) {
		debug->set_mode(p_mode);
	}
}

int NaniteServer::get_debug_mode() const {
	return debug ? debug->get_mode() : 0;
}

void NaniteServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_mesh", "resource"), &NaniteServer::register_mesh);
	ClassDB::bind_method(D_METHOD("unregister_mesh", "rid"), &NaniteServer::unregister_mesh);
	ClassDB::bind_method(D_METHOD("register_instance", "instance"), &NaniteServer::register_instance);
	ClassDB::bind_method(D_METHOD("unregister_instance", "instance"), &NaniteServer::unregister_instance);
	ClassDB::bind_method(D_METHOD("get_instance_count"), &NaniteServer::get_instance_count);
	ClassDB::bind_method(D_METHOD("get_visible_cluster_count"), &NaniteServer::get_visible_cluster_count);
	ClassDB::bind_method(D_METHOD("set_debug_mode", "mode"), &NaniteServer::set_debug_mode);
	ClassDB::bind_method(D_METHOD("get_debug_mode"), &NaniteServer::get_debug_mode);
	ClassDB::bind_method(D_METHOD("get_shadow_mode"), &NaniteServer::get_shadow_mode);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "debug_mode"), "set_debug_mode", "get_debug_mode");
}
