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

// ---- Stage 0 API: DisplayMode + LODMode orthogonal axes -----------------

void NaniteDebug::set_display_mode(int p_mode) {
	DisplayMode m = (DisplayMode)p_mode;
	switch (m) {
		case NORMAL:
		case NORMAL_WIREFRAME:
		case CLUSTER_SOLID:
		case CLUSTER_SOLID_WIREFRAME:
		case WIREFRAME_ONLY:
		case CLUSTER_SOLID_WITH_PARTITION_BORDER:
			display_mode = m;
			break;
		default:
			display_mode = NORMAL;
			break;
	}
}

int NaniteDebug::get_display_mode() const {
	return (int)display_mode;
}

void NaniteDebug::set_lod_mode(int p_mode) {
	LODMode m = (LODMode)p_mode;
	switch (m) {
		case NANITE_AUTO:
		case FORCE_LOD_LEVEL:
			lod_mode = m;
			break;
		default:
			lod_mode = FORCE_LOD_LEVEL;
			break;
	}
}

int NaniteDebug::get_lod_mode() const {
	return (int)lod_mode;
}

void NaniteDebug::set_force_lod_level(int p_level) {
	if (p_level < 0) {
		p_level = 0;
	}
	force_lod_level = p_level;
}

void NaniteDebug::set_show_bounds(bool p_show) {
	show_bounds = p_show;
}

// ---- Legacy Stage 1 GPU pipeline API ------------------------------------

void NaniteDebug::set_mode(int p_mode) {
	mode = (DebugMode)p_mode;
}

int NaniteDebug::get_mode() const {
	return (int)mode;
}

void NaniteDebug::set_wireframe(bool p_wireframe) {
	wireframe = p_wireframe;
}

// ---- ClassDB binding ----------------------------------------------------

void NaniteDebug::_bind_methods() {
	// Stage 0 API — two orthogonal axes.
	ClassDB::bind_method(D_METHOD("set_display_mode", "mode"), &NaniteDebug::set_display_mode);
	ClassDB::bind_method(D_METHOD("get_display_mode"), &NaniteDebug::get_display_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "display_mode", PROPERTY_HINT_ENUM,
					   "Normal,Normal + Wireframe,Cluster Solid,Cluster Solid + Wireframe,Wireframe Only,Cluster Solid + Partition Border"),
			"set_display_mode", "get_display_mode");

	ClassDB::bind_method(D_METHOD("set_lod_mode", "mode"), &NaniteDebug::set_lod_mode);
	ClassDB::bind_method(D_METHOD("get_lod_mode"), &NaniteDebug::get_lod_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_mode", PROPERTY_HINT_ENUM,
					   "Nanite (auto cull + LOD),Force LOD Level"),
			"set_lod_mode", "get_lod_mode");

	ClassDB::bind_method(D_METHOD("set_force_lod_level", "level"), &NaniteDebug::set_force_lod_level);
	ClassDB::bind_method(D_METHOD("get_force_lod_level"), &NaniteDebug::get_force_lod_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "force_lod_level", PROPERTY_HINT_RANGE, "0,63,1"),
			"set_force_lod_level", "get_force_lod_level");

	ClassDB::bind_method(D_METHOD("set_show_bounds", "show_bounds"), &NaniteDebug::set_show_bounds);
	ClassDB::bind_method(D_METHOD("get_show_bounds"), &NaniteDebug::get_show_bounds);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_bounds"), "set_show_bounds", "get_show_bounds");

	// Legacy Stage 1 GPU pipeline API.
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &NaniteDebug::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &NaniteDebug::get_mode);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM,
					   "None,Cluster Solid Color,LOD Solid Color,Overdraw Heatmap,Page Residency,HZB Mip Levels,HZB Occlusion"),
			"set_mode", "get_mode");

	ClassDB::bind_method(D_METHOD("set_wireframe", "wireframe"), &NaniteDebug::set_wireframe);
	ClassDB::bind_method(D_METHOD("get_wireframe"), &NaniteDebug::get_wireframe);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "wireframe"), "set_wireframe", "get_wireframe");

	// DisplayMode enum.
	BIND_ENUM_CONSTANT(NORMAL);
	BIND_ENUM_CONSTANT(NORMAL_WIREFRAME);
	BIND_ENUM_CONSTANT(CLUSTER_SOLID);
	BIND_ENUM_CONSTANT(CLUSTER_SOLID_WIREFRAME);
	BIND_ENUM_CONSTANT(WIREFRAME_ONLY);
	BIND_ENUM_CONSTANT(CLUSTER_SOLID_WITH_PARTITION_BORDER);

	// LODMode enum.
	BIND_ENUM_CONSTANT(NANITE_AUTO);
	BIND_ENUM_CONSTANT(FORCE_LOD_LEVEL);

	// Legacy DebugMode enum (names preserved for Stage 1 compat).
	BIND_ENUM_CONSTANT(NONE);
	BIND_ENUM_CONSTANT(CLUSTER_SOLID_COLOR);
	BIND_ENUM_CONSTANT(LOD_SOLID_COLOR);
	BIND_ENUM_CONSTANT(OVERDRAW_HEATMAP);
	BIND_ENUM_CONSTANT(PAGE_RESIDENCY);
	BIND_ENUM_CONSTANT(HZB_MIP_LEVELS);
	BIND_ENUM_CONSTANT(HZB_OCCLUSION);
}
