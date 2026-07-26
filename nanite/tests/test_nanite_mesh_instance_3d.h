/**************************************************************************/
/*  test_nanite_mesh_instance_3d.h                                        */
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

#include "nanite/scene/nanite_mesh_instance_3d.h"
#include "nanite/core/nanite_server.h"
#include "nanite/core/nanite_resource.h"
#include "tests/test_macros.h"

namespace TestNaniteMeshInstance3D {

TEST_CASE("[Nanite][Instance] class_registration") {
	CHECK(ClassDB::class_exists("NaniteMeshInstance3D"));
	CHECK(ClassDB::is_parent_class("NaniteMeshInstance3D", "MeshInstance3D"));
}

TEST_CASE("[Nanite][Instance] forced_lod_getter_setter") {
	// Can't instantiate Node in headless test easily, but verify the property exists
	// via ClassDB. Full instantiation test requires a SceneTree.
	// For now, just verify the class is properly registered.
	CHECK(ClassDB::class_has_method("NaniteMeshInstance3D", "set_forced_lod"));
	CHECK(ClassDB::class_has_method("NaniteMeshInstance3D", "get_forced_lod"));
}

} // namespace TestNaniteMeshInstance3D
