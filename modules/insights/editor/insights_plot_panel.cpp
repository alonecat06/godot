/**************************************************************************/
/*  insights_plot_panel.cpp                                               */
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

#include "modules/insights/editor/insights_plot_panel.h"

#include "modules/insights/insights_core/insights_database.h"

InsightsPlotPanel::PlotCanvas::PlotCanvas() {
	set_clip_contents(true);
	set_custom_minimum_size(Size2(640, 300));
	set_h_size_flags(SIZE_EXPAND_FILL);
	set_v_size_flags(SIZE_EXPAND_FILL);
}

void InsightsPlotPanel::PlotCanvas::_notification(int p_what) {
	if (p_what == NOTIFICATION_DRAW) {
		InsightsPlotPanel *panel = Object::cast_to<InsightsPlotPanel>(get_parent());
		if (!panel || panel->database.is_null() || panel->selected_plot.is_empty()) {
			return;
		}

		Array plot_data = panel->database->get_plot_data(panel->selected_plot);
		if (plot_data.size() == 0) {
			return;
		}

		// Extract data points.
		Vector<double> x_values;
		Vector<double> y_values;
		for (int i = 0; i < plot_data.size(); i++) {
			Dictionary point = plot_data[i];
			x_values.push_back(point["x"]);
			y_values.push_back(point["y"]);
		}

		// Compute ranges.
		double x_min = x_values[0];
		double x_max = x_values[0];
		double y_min = y_values[0];
		double y_max = y_values[0];
		for (int i = 1; i < x_values.size(); i++) {
			if (x_values[i] < x_min) {
				x_min = x_values[i];
			}
			if (x_values[i] > x_max) {
				x_max = x_values[i];
			}
			if (y_values[i] < y_min) {
				y_min = y_values[i];
			}
			if (y_values[i] > y_max) {
				y_max = y_values[i];
			}
		}

		if (x_max == x_min) {
			x_max = x_min + 1.0;
		}
		if (y_max == y_min) {
			y_max = y_min + 1.0;
		}

		// Add 10% margin on Y axis.
		double y_margin = (y_max - y_min) * 0.1;
		y_min -= y_margin;
		y_max += y_margin;

		Size2 size = get_size();
		float margin_left = 60.0;
		float margin_bottom = 30.0;
		float margin_top = 10.0;
		float margin_right = 10.0;

		float plot_w = size.width - margin_left - margin_right;
		float plot_h = size.height - margin_top - margin_bottom;

		if (plot_w <= 0 || plot_h <= 0) {
			return;
		}

		// Draw axes.
		Color axis_color(0.5, 0.5, 0.5);
		draw_line(Point2(margin_left, margin_top), Point2(margin_left, margin_top + plot_h), axis_color);
		draw_line(Point2(margin_left, margin_top + plot_h), Point2(margin_left + plot_w, margin_top + plot_h), axis_color);

		// Draw Y axis ticks (5 ticks).
		Ref<Font> font = get_theme_font(SNAME("font"));
		int font_size = get_theme_font_size(SNAME("font_size"));
		Color tick_color(0.7, 0.7, 0.7);
		Color text_color(0.8, 0.8, 0.8);
		int tick_count = 5;
		for (int i = 0; i <= tick_count; i++) {
			double val = y_min + (y_max - y_min) * i / tick_count;
			float y = margin_top + plot_h - (float)(i) / tick_count * plot_h;
			draw_line(Point2(margin_left - 4, y), Point2(margin_left, y), tick_color);
			draw_string(font, Point2(2, y + font_size / 3), vformat("%.1f", val), HORIZONTAL_ALIGNMENT_LEFT, margin_left - 6, font_size, text_color);
		}

		// Draw X axis ticks.
		for (int i = 0; i <= tick_count; i++) {
			double val = x_min + (x_max - x_min) * i / tick_count;
			float x = margin_left + (float)i / tick_count * plot_w;
			draw_line(Point2(x, margin_top + plot_h), Point2(x, margin_top + plot_h + 4), tick_color);
			draw_string(font, Point2(x - 20, margin_top + plot_h + font_size + 4), vformat("%.1f", val), HORIZONTAL_ALIGNMENT_LEFT, 40, font_size, text_color);
		}

		// Draw line chart.
		Color line_color(0.27, 0.67, 0.78);
		for (int i = 1; i < x_values.size(); i++) {
			float x0 = margin_left + (float)((x_values[i - 1] - x_min) / (x_max - x_min)) * plot_w;
			float y0 = margin_top + plot_h - (float)((y_values[i - 1] - y_min) / (y_max - y_min)) * plot_h;
			float x1 = margin_left + (float)((x_values[i] - x_min) / (x_max - x_min)) * plot_w;
			float y1 = margin_top + plot_h - (float)((y_values[i] - y_min) / (y_max - y_min)) * plot_h;
			draw_line(Point2(x0, y0), Point2(x1, y1), line_color, 2.0);
		}

		// Draw data points.
		Color point_color(0.96, 0.42, 0.23);
		for (int i = 0; i < x_values.size(); i++) {
			float x = margin_left + (float)((x_values[i] - x_min) / (x_max - x_min)) * plot_w;
			float y = margin_top + plot_h - (float)((y_values[i] - y_min) / (y_max - y_min)) * plot_h;
			draw_circle(Point2(x, y), 3.0, point_color);
		}
	}
}

InsightsPlotPanel::InsightsPlotPanel() {
	plot_selector = memnew(OptionButton);
	plot_selector->set_h_size_flags(SIZE_EXPAND_FILL);
	plot_selector->connect("item_selected", callable_mp(this, &InsightsPlotPanel::_on_plot_selected));
	add_child(plot_selector);

	plot_canvas = memnew(PlotCanvas);
	add_child(plot_canvas);
}

InsightsPlotPanel::~InsightsPlotPanel() {
}

void InsightsPlotPanel::update_data(const Ref<InsightsDatabase> &p_db) {
	database = p_db;

	plot_selector->clear();
	selected_plot = String();

	if (p_db.is_null()) {
		plot_canvas->queue_redraw();
		return;
	}

	Array plot_names = p_db->get_plot_names();
	for (int i = 0; i < plot_names.size(); i++) {
		String name = plot_names[i];
		plot_selector->add_item(name);
	}

	// Auto-select the first plot.
	if (plot_names.size() > 0) {
		selected_plot = plot_names[0];
		plot_selector->select(0);
	}

	plot_canvas->queue_redraw();
}

void InsightsPlotPanel::_on_plot_selected(int p_index) {
	selected_plot = plot_selector->get_item_text(p_index);
	plot_canvas->queue_redraw();
}

void InsightsPlotPanel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update_data", "db"), &InsightsPlotPanel::update_data);
	ClassDB::bind_method(D_METHOD("_on_plot_selected", "index"), &InsightsPlotPanel::_on_plot_selected);
}
