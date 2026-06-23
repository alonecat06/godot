/**************************************************************************/
/*  insights_channel.cpp                                                  */
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

#include "insights_channel.h"

#include "core/object/class_db.h"
#include "core/variant/binder_common.h"

VARIANT_ENUM_CAST(InsightsChannel::ChannelCategory);

void InsightsChannel::set_name(const StringName &p_name) {
	name = p_name;
}

StringName InsightsChannel::get_name() const {
	return name;
}

void InsightsChannel::set_color(const Color &p_color) {
	color = p_color;
}

Color InsightsChannel::get_color() const {
	return color;
}

void InsightsChannel::set_category(InsightsChannel::ChannelCategory p_category) {
	category = p_category;
	color = get_category_color(p_category);
}

InsightsChannel::ChannelCategory InsightsChannel::get_category() const {
	return category;
}

void InsightsChannel::set_is_enabled(bool p_enabled) {
	is_enabled = p_enabled;
}

bool InsightsChannel::get_is_enabled() const {
	return is_enabled;
}

void InsightsChannel::on_event(const Dictionary &p_event_data) {
	// Default: no-op. Subclasses override to process events.
}

Dictionary InsightsChannel::serialize() {
	Dictionary dict;
	dict["name"] = name;
	dict["color"] = color;
	dict["category"] = category;
	dict["is_enabled"] = is_enabled;
	return dict;
}

bool InsightsChannel::validate_zone_name(const String &p_name) {
	// Must start with "godot:" and have at least one "/" after it.
	if (!p_name.begins_with("godot:")) {
		return false;
	}
	String after_prefix = p_name.substr(6); // Skip "godot:"
	if (after_prefix.is_empty()) {
		return false;
	}
	// Must contain at least one "/" to form a path like "channel/sub".
	if (!after_prefix.contains("/")) {
		return false;
	}
	return true;
}

InsightsChannel::ChannelCategory InsightsChannel::get_category_from_zone(const String &p_zone_name) {
	if (!p_zone_name.begins_with("godot:")) {
		return CHANNEL_CATEGORY_CUSTOM;
	}
	String after_prefix = p_zone_name.substr(6);
	// Extract the first path segment (before the first "/").
	int slash_pos = after_prefix.find("/");
	String first_segment = (slash_pos >= 0) ? after_prefix.substr(0, slash_pos) : after_prefix;

	// Map first segment to category.
	if (first_segment == "physics" || first_segment == "main" || first_segment == "audio" ||
			first_segment == "rendering" || first_segment == "navigation") {
		return CHANNEL_CATEGORY_CPU;
	} else if (first_segment == "gpu") {
		return CHANNEL_CATEGORY_GPU;
	} else if (first_segment == "mem") {
		return CHANNEL_CATEGORY_MEMORY;
	} else if (first_segment == "script") {
		return CHANNEL_CATEGORY_SCRIPT;
	} else if (first_segment == "loading") {
		return CHANNEL_CATEGORY_LOADING;
	} else if (first_segment == "network") {
		return CHANNEL_CATEGORY_NETWORK;
	} else if (first_segment == "log") {
		return CHANNEL_CATEGORY_LOG;
	}
	return CHANNEL_CATEGORY_CUSTOM;
}

Color InsightsChannel::get_category_color(InsightsChannel::ChannelCategory p_category) {
	switch (p_category) {
		case CHANNEL_CATEGORY_CPU:
			return Color(0x7A / 255.0f, 0xC0 / 255.0f, 0xE5 / 255.0f);
		case CHANNEL_CATEGORY_GPU:
			return Color(0xC0 / 255.0f, 0xE5 / 255.0f, 0x7A / 255.0f);
		case CHANNEL_CATEGORY_MEMORY:
			return Color(0xE5 / 255.0f, 0x7A / 255.0f, 0xC0 / 255.0f);
		case CHANNEL_CATEGORY_SCRIPT:
			return Color(0x47 / 255.0f, 0x8C / 255.0f, 0xBF / 255.0f);
		case CHANNEL_CATEGORY_LOADING:
			return Color(0xE5 / 255.0f, 0xC7 / 255.0f, 0x7A / 255.0f);
		case CHANNEL_CATEGORY_NETWORK:
			return Color(0xE5 / 255.0f, 0xE5 / 255.0f, 0x7A / 255.0f);
		case CHANNEL_CATEGORY_LOG:
			return Color(0xC0 / 255.0f, 0xC0 / 255.0f, 0xC0 / 255.0f);
		case CHANNEL_CATEGORY_CUSTOM:
		default:
			return Color(0xFF / 255.0f, 0x69 / 255.0f, 0xB4 / 255.0f);
	}
}

void InsightsChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_name", "name"), &InsightsChannel::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &InsightsChannel::get_name);
	ClassDB::bind_method(D_METHOD("set_color", "color"), &InsightsChannel::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &InsightsChannel::get_color);
	ClassDB::bind_method(D_METHOD("set_category", "category"), &InsightsChannel::set_category);
	ClassDB::bind_method(D_METHOD("get_category"), &InsightsChannel::get_category);
	ClassDB::bind_method(D_METHOD("set_is_enabled", "is_enabled"), &InsightsChannel::set_is_enabled);
	ClassDB::bind_method(D_METHOD("get_is_enabled"), &InsightsChannel::get_is_enabled);

	ClassDB::bind_method(D_METHOD("on_event", "event_data"), &InsightsChannel::on_event);
	ClassDB::bind_method(D_METHOD("serialize"), &InsightsChannel::serialize);

	ClassDB::bind_static_method("InsightsChannel", D_METHOD("validate_zone_name", "name"), &InsightsChannel::validate_zone_name);
	ClassDB::bind_static_method("InsightsChannel", D_METHOD("get_category_from_zone", "zone_name"), &InsightsChannel::get_category_from_zone);
	ClassDB::bind_static_method("InsightsChannel", D_METHOD("get_category_color", "category"), &InsightsChannel::get_category_color);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "category", PROPERTY_HINT_ENUM, "CPU,GPU,Memory,Script,Loading,Network,Log,Custom"), "set_category", "get_category");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_enabled"), "set_is_enabled", "get_is_enabled");

	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_CPU);
	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_GPU);
	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_MEMORY);
	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_SCRIPT);
	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_LOADING);
	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_NETWORK);
	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_LOG);
	BIND_ENUM_CONSTANT(CHANNEL_CATEGORY_CUSTOM);
}

InsightsChannel::InsightsChannel() :
		color(get_category_color(CHANNEL_CATEGORY_CPU)) {
}

InsightsChannel::~InsightsChannel() {
}
