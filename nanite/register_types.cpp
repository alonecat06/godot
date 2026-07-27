/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "nanite/core/builder_config.h"
#include "nanite/core/nanite_builder.h"
#include "nanite/core/nanite_debug.h"
#include "nanite/core/nanite_page_cache.h"
#include "nanite/core/nanite_resource.h"
#include "nanite/core/nanite_server.h"
#include "nanite/scene/nanite_mesh_instance_3d.h"
#include "core/config/engine.h"
#include "core/object/class_db.h"

#if defined(NANITE_BRIDGE_GDEXT)
#include "nanite/bridge/nanite_gdext_bridge.h"
#include "nanite/bridge/nanite_gdext_bridge_manager.h" // Task 1.17.9
#endif

#ifdef TOOLS_ENABLED
#include "nanite/editor/nanite_conversion_menu.h"
#include "nanite/editor/nanite_editor_plugin.h"
#include "nanite/editor/nanite_mesh_editor.h"
#include "nanite/editor/nanite_resource_editor_window.h" // Task 0.13.4
#include "nanite/editor/nanite_resource_preview_gen.h"
#include "editor/plugins/editor_plugin.h"
#endif // TOOLS_ENABLED

void initialize_nanite_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		// Register the runtime classes and instantiate the singleton.
		// SERVERS runs before SCENE, so NaniteServer is ready by the
		// time any NaniteMeshInstance3D / resource is constructed.
		ClassDB::register_class<NaniteDebug>();
		ClassDB::register_class<NanitePageCache>();
		ClassDB::register_class<NaniteServer>();

		NaniteServer *ns = memnew(NaniteServer);
		Engine::get_singleton()->add_singleton(Engine::Singleton("NaniteServer", ns));
		// Task 1.17.2 — core init no longer creates any concrete bridge;
		// `bridge` stays nullptr until the bridge layer's Manager injects
		// it via set_bridge() at SCENE level (see below).
		ns->init();
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<BuilderConfig>();
		ClassDB::register_class<NaniteBuilder>();
		ClassDB::register_class<NaniteMeshResource>();
		ClassDB::register_class<NaniteMeshInstance3D>();

		// Phase-2 init for the core: RenderingDevice is now available
		// (DisplayServer::create ran between SERVERS and SCENE module init
		// in main.cpp setup2()). This is where gpu_pipeline gets created;
		// doing it here — rather than lazily in set_bridge() — keeps
		// pipeline init and bridge injection as independent concerns.
		NaniteServer *ns = NaniteServer::get_singleton();
		if (ns != nullptr) {
			ns->init_engine_post();
		}

#if defined(NANITE_BRIDGE_GDEXT)
		// CompositorEffect is itself registered at SCENE level (see
		// scene/register_scene_types.cpp), so the gdext bridge must be
		// registered here too.
		ClassDB::register_class<NaniteGDExtBridge>();

		// Task 1.17.9 — bridge layer's Manager singleton. Lives in
		// nanite/bridge/ and owns the two NaniteGDExtBridge CompositorEffects
		// + the pre-baked default_compositor Resource. The Manager injects
		// itself into NaniteServer via set_bridge() and wires SceneTree
		// signals for automatic viewport attachment.
		//
		// IMPORTANT: must be created at SCENE level, NOT SERVERS level.
		// RenderingServer is created in Main::setup2() AFTER
		// initialize_modules(SERVERS) but BEFORE initialize_modules(SCENE).
		// The Compositor / CompositorEffect constructors call
		// RenderingServer::get_singleton()->compositor_create() /
		// compositor_effect_create(), and Compositor::set_compositor_effects()
		// calls RenderingServer::get_singleton()->compositor_set_compositor_effects()
		// — all crash if RS is nullptr. At SCENE level RS is guaranteed ready.
		//
		// NOTE: capture the pointer from memnew — `singleton` is only set
		// INSIDE init() (singleton = this), so get_singleton() would
		// return nullptr before init() runs. Calling init() on nullptr
		// crashes on the first member write (e.g. auto_attach_enabled).
		if (ns != nullptr) {
			NaniteGDExtBridgeManager *mgr = memnew(NaniteGDExtBridgeManager);
			mgr->init(ns);
		}
#endif
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::register_class<NaniteMeshEditor>();
		ClassDB::register_class<NaniteMeshResourceEditorWindow>(); // Task 0.13.4
		ClassDB::register_class<EditorInspectorPluginNanite>();
		ClassDB::register_class<NaniteResourcePreviewGenerator>();
		ClassDB::register_class<NaniteEditorPlugin>();
		ClassDB::register_class<NaniteConversionContextMenu>(); // Task 0.12.3
		// EditorPlugins::add_by_type registers NaniteEditorPlugin with the
		// EditorNode so it is constructed during editor startup, which in
		// turn registers the inspector plugin + preview generator +
		// context menu plugin (Task 0.12.3).
		EditorPlugins::add_by_type<NaniteEditorPlugin>();
	}
#endif // TOOLS_ENABLED
}

void uninitialize_nanite_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SERVERS) {
		// Tear down the singleton. Capture the pointer first because
		// finish() clears NaniteServer::singleton.
		NaniteServer *ns = NaniteServer::get_singleton();
		if (ns) {
			ns->finish();
			memdelete(ns);
		}
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
#if defined(NANITE_BRIDGE_GDEXT)
		// Task 1.17.9 — tear down the bridge layer's Manager. Must run at
		// SCENE level to match where it was created (initialize order:
		// SCENE init → ... → SCENE deinit → SERVERS deinit). The Manager
		// calls set_bridge(nullptr) on NaniteServer (still alive at this
		// point — SERVERS deinit happens later).
		// NOTE: capture the pointer before calling finish() — finish()
		// clears `singleton` (singleton = nullptr) as its last step, so
		// get_singleton() would return nullptr after finish() and
		// memdelete(nullptr) would crash.
		NaniteGDExtBridgeManager *mgr = NaniteGDExtBridgeManager::get_singleton();
		if (mgr != nullptr) {
			mgr->finish();
			memdelete(mgr);
		}
#endif
		return;
	}
}
