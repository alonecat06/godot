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

#include "core/builder_config.h"
#include "core/nanite_builder.h"
#include "core/nanite_debug.h"
#include "core/nanite_page_cache.h"
#include "core/nanite_resource.h"
#include "core/nanite_server.h"
#include "scene/nanite_mesh_instance_3d.h"
#include "core/config/engine.h"
#include "core/object/class_db.h"

#if defined(NANITE_BRIDGE_GDEXT)
#include "bridge/nanite_gdext_bridge.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/nanite_conversion_menu.h"
#include "editor/nanite_editor_plugin.h"
#include "editor/nanite_mesh_editor.h"
#include "editor/nanite_resource_editor_window.h" // Task 0.13.4
#include "editor/nanite_resource_preview_gen.h"
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
		ns->init();
		return;
	}

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<BuilderConfig>();
		ClassDB::register_class<NaniteBuilder>();
		ClassDB::register_class<NaniteMeshResource>();
		ClassDB::register_class<NaniteMeshInstance3D>();
#if defined(NANITE_BRIDGE_GDEXT)
		// CompositorEffect is itself registered at SCENE level (see
		// scene/register_scene_types.cpp), so the gdext bridge must be
		// registered here too — NaniteServer (SERVERS level) only memnew's
		// the instances; it does not require the class to be registered.
		ClassDB::register_class<NaniteGDExtBridge>();
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

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
