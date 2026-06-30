/**************************************************************************/
/*  custom_channel.cpp                                                    */
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

#include "modules/insights/channels/custom_channel.h"

#include "core/object/class_db.h"
#include "core/variant/binder_common.h"
#include "core/variant/typed_array.h"
#include "core/os/os.h"

void CustomChannel::begin_zone(const String &p_name, const String &p_file, int p_line) {
	if (zones.size() >= max_zones) {
		zones.remove_at(0);
	}

	CustomZone zone;
	zone.name = p_name;
	zone.channel = get_name();
	zone.file = p_file;
	zone.line = p_line;
	zone.start_ns = OS::get_singleton()->get_ticks_usec() * 1000;
	zone.end_ns = 0;

	zones.push_back(zone);
}

void CustomChannel::end_zone() {
	for (int i = zones.size() - 1; i >= 0; i--) {
		if (zones[i].end_ns == 0) {
			zones[i].end_ns = OS::get_singleton()->get_ticks_usec() * 1000;
			break;
		}
	}
}

TypedArray<Dictionary> CustomChannel::get_zones() const {
	TypedArray<Dictionary> arr;
	arr.resize(zones.size());
	for (uint32_t i = 0; i < zones.size(); i++) {
		Dictionary d;
		d["name"] = zones[i].name;
		d["channel"] = zones[i].channel;
		d["file"] = zones[i].file;
		d["line"] = zones[i].line;
		d["start_ns"] = zones[i].start_ns;
		d["end_ns"] = zones[i].end_ns;
		arr[i] = d;
	}
	return arr;
}

void CustomChannel::set_max_zones(uint32_t p_max) {
	max_zones = p_max;
}

uint32_t CustomChannel::get_max_zones() const {
	return max_zones;
}

void CustomChannel::on_event(const Dictionary &p_event_data) {
	if (!p_event_data.has("type")) {
		return;
	}
	String type = p_event_data["type"];
	if (type == "begin_zone") {
		String name = p_event_data.get("name", "");
		String file = p_event_data.get("file", "");
		int line = p_event_data.get("line", 0);
		begin_zone(name, file, line);
	} else if (type == "end_zone") {
		end_zone();
	}
}

Dictionary CustomChannel::serialize() {
	Dictionary dict;
	dict["zone_count"] = (int)zones.size();
	return dict;
}

void CustomChannel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("begin_zone", "name", "file", "line"), &CustomChannel::begin_zone, DEFVAL(""), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("end_zone"), &CustomChannel::end_zone);
	ClassDB::bind_method(D_METHOD("get_zones"), &CustomChannel::get_zones);
	ClassDB::bind_method(D_METHOD("set_max_zones", "max"), &CustomChannel::set_max_zones);
	ClassDB::bind_method(D_METHOD("get_max_zones"), &CustomChannel::get_max_zones);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_zones"), "set_max_zones", "get_max_zones");
}

CustomChannel::CustomChannel() {
	set_category(CHANNEL_CATEGORY_CUSTOM);
	set_color(Color(0xE5 / 255.0f, 0x7A / 255.0f, 0xE5 / 255.0f));
	set_name("custom");
}

CustomChannel::~CustomChannel() {
}
