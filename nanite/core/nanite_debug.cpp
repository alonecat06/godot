/**************************************************************************/
/*  nanite_debug.cpp                                                       */
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

#include "nanite/core/nanite_debug.h"

#include "core/object/class_db.h"

void NaniteDebug::set_mode(int p_mode) {
	mode = (DebugMode)p_mode;
}

int NaniteDebug::get_mode() const {
	return (int)mode;
}

void NaniteDebug::set_wireframe(bool p_wireframe) {
	wireframe = p_wireframe;
}

bool NaniteDebug::get_wireframe() const {
	return wireframe;
}

void NaniteDebug::set_show_bounds(bool p_show_bounds) {
	show_bounds = p_show_bounds;
}

bool NaniteDebug::get_show_bounds() const {
	return show_bounds;
}

void NaniteDebug::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &NaniteDebug::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &NaniteDebug::get_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "None,Cluster Solid Color,LOD Solid Color,Overdraw Heatmap,Page Residency,HZB Mip Levels,HZB Occlusion"), "set_mode", "get_mode");

	ClassDB::bind_method(D_METHOD("set_wireframe", "wireframe"), &NaniteDebug::set_wireframe);
	ClassDB::bind_method(D_METHOD("get_wireframe"), &NaniteDebug::get_wireframe);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "wireframe"), "set_wireframe", "get_wireframe");

	ClassDB::bind_method(D_METHOD("set_show_bounds", "show_bounds"), &NaniteDebug::set_show_bounds);
	ClassDB::bind_method(D_METHOD("get_show_bounds"), &NaniteDebug::get_show_bounds);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_bounds"), "set_show_bounds", "get_show_bounds");

	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(CLUSTER_SOLID_COLOR);
	BIND_ENUM_CONSTANT(LOD_SOLID_COLOR);
	BIND_ENUM_CONSTANT(OVERDRAW_HEATMAP);
	BIND_ENUM_CONSTANT(PAGE_RESIDENCY);
	BIND_ENUM_CONSTANT(HZB_MIP_LEVELS);
	BIND_ENUM_CONSTANT(HZB_OCCLUSION);
}
