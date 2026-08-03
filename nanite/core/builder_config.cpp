/**************************************************************************/
/*  builder_config.cpp                                                    */
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

#include "builder_config.h"

#include "core/object/class_db.h"

void BuilderConfig::set_max_vertices(int p_value) {
	max_vertices = p_value;
}

int BuilderConfig::get_max_vertices() const {
	return max_vertices;
}

void BuilderConfig::set_max_triangles(int p_value) {
	max_triangles = p_value;
}

int BuilderConfig::get_max_triangles() const {
	return max_triangles;
}

void BuilderConfig::set_min_triangles(int p_value) {
	min_triangles = p_value;
}

int BuilderConfig::get_min_triangles() const {
	return min_triangles;
}

void BuilderConfig::set_partition_size(int p_value) {
	partition_size = p_value;
}

int BuilderConfig::get_partition_size() const {
	return partition_size;
}

void BuilderConfig::set_cone_weight(double p_value) {
	cone_weight = p_value;
}

double BuilderConfig::get_cone_weight() const {
	return cone_weight;
}

void BuilderConfig::set_split_factor(double p_value) {
	split_factor = p_value;
}

double BuilderConfig::get_split_factor() const {
	return split_factor;
}

void BuilderConfig::set_simplification_ratio(double p_value) {
	simplification_ratio = p_value;
}

double BuilderConfig::get_simplification_ratio() const {
	return simplification_ratio;
}

void BuilderConfig::set_target_error(double p_value) {
	target_error = p_value;
}

double BuilderConfig::get_target_error() const {
	return target_error;
}

void BuilderConfig::set_lock_partition_border(bool p_value) {
	lock_partition_border = p_value;
}

bool BuilderConfig::get_lock_partition_border() const {
	return lock_partition_border;
}

void BuilderConfig::set_meshlet_optimize_level(int p_value) {
	meshlet_optimize_level = p_value;
}

int BuilderConfig::get_meshlet_optimize_level() const {
	return meshlet_optimize_level;
}

void BuilderConfig::set_max_lod_levels(int p_value) {
	max_lod_levels = p_value;
}

int BuilderConfig::get_max_lod_levels() const {
	return max_lod_levels;
}

void BuilderConfig::set_page_size_bytes(int p_value) {
	page_size_bytes = p_value;
}

int BuilderConfig::get_page_size_bytes() const {
	return page_size_bytes;
}

void BuilderConfig::set_shadow_lod_depth(int p_value) {
	shadow_lod_depth = p_value;
}

int BuilderConfig::get_shadow_lod_depth() const {
	return shadow_lod_depth;
}

void BuilderConfig::set_optimize_size(bool p_value) {
	optimize_size = p_value;
}

bool BuilderConfig::get_optimize_size() const {
	return optimize_size;
}

bool BuilderConfig::is_valid() const {
	if (max_vertices < 32) {
		return false;
	}
	if (max_triangles < 32) {
		return false;
	}
	if (partition_size < 2) {
		return false;
	}
	if (page_size_bytes < 4096) {
		return false;
	}
	if (shadow_lod_depth < 1) {
		return false;
	}
	return true;
}

void BuilderConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_max_vertices", "max_vertices"), &BuilderConfig::set_max_vertices);
	ClassDB::bind_method(D_METHOD("get_max_vertices"), &BuilderConfig::get_max_vertices);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_vertices", PROPERTY_HINT_RANGE, "32,256,1"), "set_max_vertices", "get_max_vertices");

	ClassDB::bind_method(D_METHOD("set_max_triangles", "max_triangles"), &BuilderConfig::set_max_triangles);
	ClassDB::bind_method(D_METHOD("get_max_triangles"), &BuilderConfig::get_max_triangles);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_triangles", PROPERTY_HINT_RANGE, "32,512,1"), "set_max_triangles", "get_max_triangles");

	ClassDB::bind_method(D_METHOD("set_min_triangles", "min_triangles"), &BuilderConfig::set_min_triangles);
	ClassDB::bind_method(D_METHOD("get_min_triangles"), &BuilderConfig::get_min_triangles);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "min_triangles", PROPERTY_HINT_RANGE, "1,128,1"), "set_min_triangles", "get_min_triangles");

	ClassDB::bind_method(D_METHOD("set_partition_size", "partition_size"), &BuilderConfig::set_partition_size);
	ClassDB::bind_method(D_METHOD("get_partition_size"), &BuilderConfig::get_partition_size);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "partition_size", PROPERTY_HINT_RANGE, "2,16,1"), "set_partition_size", "get_partition_size");

	ClassDB::bind_method(D_METHOD("set_cone_weight", "cone_weight"), &BuilderConfig::set_cone_weight);
	ClassDB::bind_method(D_METHOD("get_cone_weight"), &BuilderConfig::get_cone_weight);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cone_weight", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_cone_weight", "get_cone_weight");

	ClassDB::bind_method(D_METHOD("set_split_factor", "split_factor"), &BuilderConfig::set_split_factor);
	ClassDB::bind_method(D_METHOD("get_split_factor"), &BuilderConfig::get_split_factor);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "split_factor", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_split_factor", "get_split_factor");

	ClassDB::bind_method(D_METHOD("set_simplification_ratio", "simplification_ratio"), &BuilderConfig::set_simplification_ratio);
	ClassDB::bind_method(D_METHOD("get_simplification_ratio"), &BuilderConfig::get_simplification_ratio);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "simplification_ratio", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_simplification_ratio", "get_simplification_ratio");

	ClassDB::bind_method(D_METHOD("set_target_error", "target_error"), &BuilderConfig::set_target_error);
	ClassDB::bind_method(D_METHOD("get_target_error"), &BuilderConfig::get_target_error);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "target_error", PROPERTY_HINT_RANGE, "0,1,0.0001"), "set_target_error", "get_target_error");

	ClassDB::bind_method(D_METHOD("set_lock_partition_border", "lock_partition_border"), &BuilderConfig::set_lock_partition_border);
	ClassDB::bind_method(D_METHOD("get_lock_partition_border"), &BuilderConfig::get_lock_partition_border);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "lock_partition_border"), "set_lock_partition_border", "get_lock_partition_border");

	ClassDB::bind_method(D_METHOD("set_meshlet_optimize_level", "meshlet_optimize_level"), &BuilderConfig::set_meshlet_optimize_level);
	ClassDB::bind_method(D_METHOD("get_meshlet_optimize_level"), &BuilderConfig::get_meshlet_optimize_level);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "meshlet_optimize_level", PROPERTY_HINT_RANGE, "0,3,1"), "set_meshlet_optimize_level", "get_meshlet_optimize_level");

	ClassDB::bind_method(D_METHOD("set_max_lod_levels", "max_lod_levels"), &BuilderConfig::set_max_lod_levels);
	ClassDB::bind_method(D_METHOD("get_max_lod_levels"), &BuilderConfig::get_max_lod_levels);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_lod_levels", PROPERTY_HINT_RANGE, "1,32,1"), "set_max_lod_levels", "get_max_lod_levels");

	ClassDB::bind_method(D_METHOD("set_page_size_bytes", "page_size_bytes"), &BuilderConfig::set_page_size_bytes);
	ClassDB::bind_method(D_METHOD("get_page_size_bytes"), &BuilderConfig::get_page_size_bytes);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "page_size_bytes", PROPERTY_HINT_RANGE, "4096,67108864,1"), "set_page_size_bytes", "get_page_size_bytes");

	ClassDB::bind_method(D_METHOD("set_shadow_lod_depth", "shadow_lod_depth"), &BuilderConfig::set_shadow_lod_depth);
	ClassDB::bind_method(D_METHOD("get_shadow_lod_depth"), &BuilderConfig::get_shadow_lod_depth);
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_lod_depth", PROPERTY_HINT_RANGE, "1,16,1"), "set_shadow_lod_depth", "get_shadow_lod_depth");

	ClassDB::bind_method(D_METHOD("set_optimize_size", "optimize_size"), &BuilderConfig::set_optimize_size);
	ClassDB::bind_method(D_METHOD("get_optimize_size"), &BuilderConfig::get_optimize_size);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "optimize_size"), "set_optimize_size", "get_optimize_size");

	ClassDB::bind_method(D_METHOD("is_valid"), &BuilderConfig::is_valid);
}
