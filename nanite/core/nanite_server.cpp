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

#include "core/nanite_server.h"

#include "core/nanite_debug.h"
#include "core/nanite_page_cache.h"
#include "core/nanite_resource.h"
#include "gpu/nanite_gpu_pipeline.h"
#include "gpu/nanite_mesh_data.h"
#include "scene/nanite_mesh_instance_3d.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/object/class_db.h"
#include "servers/rendering/rendering_device.h"

#if defined(NANITE_BRIDGE_GDEXT)
#include "bridge/nanite_gdext_bridge.h"
#include "scene/resources/compositor.h" // CompositorEffect::EffectCallbackType
#endif

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
	// active. GLOBAL_DEF is idempotent: it only registers the default
	// the first time, so calling it here on every init() is safe.
	GLOBAL_DEF(PropertyInfo(Variant::STRING, "nanite/bridge/active", PROPERTY_HINT_ENUM, "gdext,module,deep"), "gdext");

	page_cache = memnew(NanitePageCache);
	debug = memnew(NaniteDebug);

	String active_bridge = GLOBAL_GET("nanite/bridge/active");

#if defined(NANITE_BRIDGE_GDEXT)
	if (active_bridge == "gdext") {
		// Two CompositorEffect instances, one per callback type. The
		// PRE_OPAQUE instance is also the "main" INaniteBridge handle
		// (install() sets the shadow mode on the server). Both Refs are
		// released in finish(); bridge aliases pre_opaque_bridge.ptr().
		pre_opaque_bridge = Ref<NaniteGDExtBridge>(memnew(NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_PRE_OPAQUE)));
		post_opaque_bridge = Ref<NaniteGDExtBridge>(memnew(NaniteGDExtBridge(CompositorEffect::EFFECT_CALLBACK_TYPE_POST_OPAQUE)));
		bridge = pre_opaque_bridge.ptr();
		if (bridge) {
			bridge->install(this);
		}
		// NOTE: the effect RIDs exist on the RenderingServer but are not
		// yet attached to any Compositor resource, so the renderer will
		// not invoke them until Stage 2 wires them into the default
		// Compositor's effects list.
	} else if (active_bridge == "module" || active_bridge == "deep") {
		ERR_PRINT(vformat("NaniteServer: bridge '%s' is not available in Stage 1 (only 'gdext' is compiled). Falling back to no bridge.", active_bridge));
	} else {
		ERR_PRINT(vformat("NaniteServer: unknown bridge '%s'. Falling back to no bridge.", active_bridge));
	}
#else
	ERR_PRINT("NaniteServer: no Nanite bridge compiled in (set nanite_bridge=gdext to enable).");
	(void)active_bridge;
#endif

	// gpu_pipeline is left nullptr until Task 1.5 wires up the real
	// GPU raster pipeline; do NOT call gpu_pipeline->init(rd) here yet.
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
		// TODO Task 1.5: gpu_pipeline->finish();
		memdelete(gpu_pipeline);
		gpu_pipeline = nullptr;
	}
	// Stage 1 gdext bridges are RefCounted (CompositorEffect is a Resource),
	// so they are managed by Ref<> — NOT memdelete'd. Clear the raw `bridge`
	// alias first (it points into pre_opaque_bridge) before dropping the Refs.
	bridge = nullptr;
	if (pre_opaque_bridge.is_valid()) {
		pre_opaque_bridge.unref();
	}
	if (post_opaque_bridge.is_valid()) {
		post_opaque_bridge.unref();
	}

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
	instance_map[((Object *)p_instance)->get_instance_id()] = p_instance;
}

void NaniteServer::unregister_instance(NaniteMeshInstance3D *p_instance) {
	ERR_FAIL_NULL(p_instance);
	instance_map.erase(((Object *)p_instance)->get_instance_id());
}

void NaniteServer::render_visibility(const RenderData *p_render_data) {
	(void)p_render_data; // Stage 1 doesn't read camera/render-target info yet.
	if (!gpu_pipeline || instance_map.size() == 0) {
		return;
	}
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (!rd || !gpu_pipeline->is_initialized()) {
		return;
	}

	// Stage 1: drive a single mesh through the GPU pipeline. Multi-mesh
	// iteration is a Stage 2 concern; for now use the first mesh in mesh_map.
	HashMap<const NaniteMeshResource *, MeshEntry>::Iterator it = mesh_map.begin();
	if (!it) {
		return;
	}
	NaniteMeshData *md = it->value.data;
	if (!md || !md->is_gpu_uploaded()) {
		return;
	}

	// Stage 1: identity camera + fixed screen size. The bridge will provide
	// real view/projection matrices and the render-target size in a later
	// task; for now use identity so dispatches don't crash.
	NaniteGPUPipeline::CullParams params;
	static const float identity[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	memcpy(params.view_matrix, identity, sizeof(identity));
	memcpy(params.projection, identity, sizeof(identity));
	params.screen_size[0] = 1280; // TODO: from render_data->get_render_scene_buffers()
	params.screen_size[1] = 720;
	params.error_threshold = 0.01f;
	params.bvh_node_count = 1; // Stage 1: BVH traversal not yet implemented
	params.cluster_count = it->key->get_cluster_count();
	if (params.cluster_count == 0) {
		return;
	}

	gpu_pipeline->ensure_screen_buffers(rd, params.screen_size[0], params.screen_size[1]);
	RID visible_buffer = gpu_pipeline->dispatch_cull(rd, params, md);
	if (visible_buffer.is_valid()) {
		// Stage 1 cull emits cluster_count as visible_count (pass-through).
		gpu_pipeline->dispatch_rasterize(rd, visible_buffer, params.cluster_count, md);
		gpu_pipeline->dispatch_hzb_build(rd, gpu_pipeline->get_depth_buffer());
	}
	visible_cluster_count = params.cluster_count;
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
	// Stage 1: dispatch material resolve using the pipeline's internal
	// vis_buffer + color_buffer. For multi-mesh, we'd loop over mesh_map;
	// for now use the first mesh_data. (Stage 2 will properly iterate
	// instances + meshes.)
	HashMap<const NaniteMeshResource *, MeshEntry>::Iterator it = mesh_map.begin();
	if (!it) {
		return;
	}
	NaniteMeshData *md = it->value.data;
	if (!md || !md->is_gpu_uploaded()) {
		return;
	}
	int debug_mode = debug ? (int)debug->get_mode_enum() : 0;
	gpu_pipeline->dispatch_material_resolve(rd, gpu_pipeline->get_vis_buffer(), md, debug_mode);
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
