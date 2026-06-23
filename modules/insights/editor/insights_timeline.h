/**************************************************************************/
/*  insights_timeline.h                                                   */
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

#include "scene/gui/control.h"
#include "modules/insights/insights_core/insights_database.h"

class InsightsTimeline : public Control {
	GDCLASS(InsightsTimeline, Control);

private:
	double current_scale = 1.0;
	double scroll_x = 0.0;
	Ref<InsightsDatabase> database;
	Dictionary selected_zone;
	Dictionary hovered_zone;
	Array cpu_zones;
	Array gpu_zones;
	Array load_zones;
	Array net_zones;
	Array frame_markers_data;

	void _load_data();
	void _draw_timeline();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_database(const Ref<InsightsDatabase> &p_db);
	Ref<InsightsDatabase> get_database() const;
	double get_current_scale() const;
	void set_current_scale(double p_scale);
	double get_scroll_x() const;
	void set_scroll_x(double p_scroll);
	uint64_t get_total_duration_ns() const;
	Rect2 get_visible_range() const;
	Dictionary get_selected_zone() const;
	void zoom_to_zone(const Dictionary &p_zone);

	InsightsTimeline();
};
