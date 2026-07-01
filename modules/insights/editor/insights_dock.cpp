/**************************************************************************/
/*  insights_dock.cpp                                                     */
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

#include "modules/insights/editor/insights_dock.h"

#ifdef TOOLS_ENABLED

#include "modules/insights/editor/insights_timeline.h"
#include "modules/insights/editor/insights_flamegraph.h"
#include "modules/insights/editor/insights_memory_panel.h"
#include "modules/insights/editor/insights_loading_panel.h"
#include "modules/insights/editor/insights_network_panel.h"
#include "modules/insights/editor/insights_compare_panel.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/editor_node.h"

InsightsDock::InsightsDock() {
	// Toolbar.
	toolbar = memnew(HBoxContainer);
	add_child(toolbar);

	btn_start = memnew(Button);
	btn_start->set_text(TTR("Start"));
	toolbar->add_child(btn_start);

	btn_stop = memnew(Button);
	btn_stop->set_text(TTR("Stop"));
	toolbar->add_child(btn_stop);

	btn_open = memnew(Button);
	btn_open->set_text(TTR("Open"));
	toolbar->add_child(btn_open);

	btn_compare = memnew(Button);
	btn_compare->set_text(TTR("Compare"));
	toolbar->add_child(btn_compare);

	btn_clear = memnew(Button);
	btn_clear->set_text(TTR("Clear"));
	toolbar->add_child(btn_clear);

	// Float button — moves Insights to its own window.
	btn_float = memnew(Button);
	btn_float->set_text(TTR("Float"));
	btn_float->set_tooltip_text(TTR("Make the Insights panel floating."));
	toolbar->add_child(btn_float);
	btn_float->connect(SceneStringName(pressed), callable_mp(this, &InsightsDock::_on_float_pressed));

	// Channel tabs.
	channel_tabs = memnew(TabContainer);
	channel_tabs->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(channel_tabs);

	// CPU tab.
	InsightsTimeline *cpu_timeline = memnew(InsightsTimeline);
	channel_tabs->add_child(cpu_timeline);
	channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("CPU"));

	// GPU tab.
	InsightsFlamegraph *gpu_flamegraph = memnew(InsightsFlamegraph);
	channel_tabs->add_child(gpu_flamegraph);
	channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("GPU"));

	// Memory tab.
	InsightsMemoryPanel *memory_panel = memnew(InsightsMemoryPanel);
	channel_tabs->add_child(memory_panel);
	channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Memory"));

	// Loading tab.
	InsightsLoadingPanel *loading_panel = memnew(InsightsLoadingPanel);
	channel_tabs->add_child(loading_panel);
	channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Loading"));

	// Network tab.
	InsightsNetworkPanel *network_panel = memnew(InsightsNetworkPanel);
	channel_tabs->add_child(network_panel);
	channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Network"));

	// Log tab.
	VBoxContainer *log_container = memnew(VBoxContainer);
	channel_tabs->add_child(log_container);
	channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Log"));

	// Compare tab.
	InsightsComparePanel *compare_panel = memnew(InsightsComparePanel);
	channel_tabs->add_child(compare_panel);
	channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Compare"));

	// Playback bar.
	playback_bar = memnew(HBoxContainer);
	add_child(playback_bar);

	Button *btn_play = memnew(Button);
	btn_play->set_text(TTR("Play"));
	playback_bar->add_child(btn_play);

	Button *btn_pause = memnew(Button);
	btn_pause->set_text(TTR("Pause"));
	playback_bar->add_child(btn_pause);

	Button *btn_step = memnew(Button);
	btn_step->set_text(TTR("Step"));
	playback_bar->add_child(btn_step);
}

void InsightsDock::_on_float_pressed() {
	EditorDock *dock = Object::cast_to<EditorDock>(get_parent());
	if (dock) {
		EditorDockManager::get_singleton()->make_dock_floating(dock);
	}
}

void InsightsDock::set_database(const Ref<InsightsDatabase> &p_db) {
	current_database = p_db;

	InsightsTimeline *timeline = get_timeline();
	if (timeline) {
		timeline->set_database(p_db);
	}

	InsightsFlamegraph *flamegraph = get_flamegraph();
	if (flamegraph) {
		flamegraph->set_database(p_db);
	}

	InsightsMemoryPanel *memory = get_memory_panel();
	if (memory) {
		memory->set_database(p_db);
	}

	InsightsLoadingPanel *loading = get_loading_panel();
	if (loading) {
		loading->set_database(p_db);
	}

	InsightsNetworkPanel *network = get_network_panel();
	if (network) {
		network->set_database(p_db);
	}
}

Ref<InsightsDatabase> InsightsDock::get_database() const {
	return current_database;
}

InsightsTimeline *InsightsDock::get_timeline() const {
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		InsightsTimeline *timeline = Object::cast_to<InsightsTimeline>(channel_tabs->get_child(i));
		if (timeline) {
			return timeline;
		}
	}
	return nullptr;
}

InsightsFlamegraph *InsightsDock::get_flamegraph() const {
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		InsightsFlamegraph *flamegraph = Object::cast_to<InsightsFlamegraph>(channel_tabs->get_child(i));
		if (flamegraph) {
			return flamegraph;
		}
	}
	return nullptr;
}

InsightsMemoryPanel *InsightsDock::get_memory_panel() const {
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		InsightsMemoryPanel *panel = Object::cast_to<InsightsMemoryPanel>(channel_tabs->get_child(i));
		if (panel) {
			return panel;
		}
	}
	return nullptr;
}

InsightsLoadingPanel *InsightsDock::get_loading_panel() const {
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		InsightsLoadingPanel *panel = Object::cast_to<InsightsLoadingPanel>(channel_tabs->get_child(i));
		if (panel) {
			return panel;
		}
	}
	return nullptr;
}

InsightsNetworkPanel *InsightsDock::get_network_panel() const {
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		InsightsNetworkPanel *panel = Object::cast_to<InsightsNetworkPanel>(channel_tabs->get_child(i));
		if (panel) {
			return panel;
		}
	}
	return nullptr;
}

InsightsComparePanel *InsightsDock::get_compare_panel() const {
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		InsightsComparePanel *panel = Object::cast_to<InsightsComparePanel>(channel_tabs->get_child(i));
		if (panel) {
			return panel;
		}
	}
	return nullptr;
}

void InsightsDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsDock::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsDock::get_database);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
}

#endif // TOOLS_ENABLED
