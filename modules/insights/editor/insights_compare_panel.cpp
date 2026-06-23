/**************************************************************************/
/*  insights_compare_panel.cpp                                            */
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

#include "insights_compare_panel.h"

#include "core/math/color.h"

InsightsComparePanel::InsightsComparePanel() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 300));

	comparator.instantiate();
}

void InsightsComparePanel::set_baseline(const Ref<InsightsDatabase> &p_db) {
	baseline_trace = p_db;
}

Ref<InsightsDatabase> InsightsComparePanel::get_baseline() const {
	return baseline_trace;
}

void InsightsComparePanel::set_current(const Ref<InsightsDatabase> &p_db) {
	current_trace = p_db;
}

Ref<InsightsDatabase> InsightsComparePanel::get_current() const {
	return current_trace;
}

Dictionary InsightsComparePanel::compute_diff() {
	if (baseline_trace.is_valid() && current_trace.is_valid()) {
		diff_data = comparator->compute_diff(baseline_trace, current_trace);
	}

	return diff_data;
}

void InsightsComparePanel::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		Size2i size = get_size();

		// Draw left panel (baseline).
		draw_rect(Rect2i(0, 0, size.width / 2, size.height), Color(0.1, 0.1, 0.15, 0.8));
		draw_string(get_theme_font(SNAME("font")), Point2i(8, 20), TTR("Baseline"), HORIZONTAL_ALIGNMENT_LEFT, size.width / 2 - 16, get_theme_font_size(SNAME("font_size")));

		// Draw right panel (current).
		draw_rect(Rect2i(size.width / 2, 0, size.width / 2, size.height), Color(0.1, 0.15, 0.1, 0.8));
		draw_string(get_theme_font(SNAME("font")), Point2i(size.width / 2 + 8, 20), TTR("Current"), HORIZONTAL_ALIGNMENT_LEFT, size.width / 2 - 16, get_theme_font_size(SNAME("font_size")));

		// Draw diff highlights.
		Array regressions = diff_data["regressions"];
		Array improvements = diff_data["improvements"];
		int y_offset = 40;
		for (int i = 0; i < regressions.size() && y_offset < size.height - 20; i++) {
			Dictionary reg = regressions[i];
			draw_rect(Rect2i(0, y_offset, size.width, 18), Color(0.6, 0.1, 0.1, 0.3));
			draw_string(get_theme_font(SNAME("font")), Point2i(8, y_offset + 14), String(reg["name"]) + " (regression)", HORIZONTAL_ALIGNMENT_LEFT, size.width - 16, get_theme_font_size(SNAME("font_size")));
			y_offset += 20;
		}
		for (int i = 0; i < improvements.size() && y_offset < size.height - 20; i++) {
			Dictionary imp = improvements[i];
			draw_rect(Rect2i(0, y_offset, size.width, 18), Color(0.1, 0.6, 0.1, 0.3));
			draw_string(get_theme_font(SNAME("font")), Point2i(8, y_offset + 14), String(imp["name"]) + " (improvement)", HORIZONTAL_ALIGNMENT_LEFT, size.width - 16, get_theme_font_size(SNAME("font_size")));
			y_offset += 20;
		}
	}
}

void InsightsComparePanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_baseline", "db"), &InsightsComparePanel::set_baseline);
	ClassDB::bind_method(D_METHOD("get_baseline"), &InsightsComparePanel::get_baseline);
	ClassDB::bind_method(D_METHOD("set_current", "db"), &InsightsComparePanel::set_current);
	ClassDB::bind_method(D_METHOD("get_current"), &InsightsComparePanel::get_current);
	ClassDB::bind_method(D_METHOD("compute_diff"), &InsightsComparePanel::compute_diff);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "baseline", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_baseline", "get_baseline");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "current", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_current", "get_current");
}
