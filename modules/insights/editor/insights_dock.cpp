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
#include "modules/insights/insights_core/insights_manager.h"
#include "core/os/time.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/editor_node.h"

InsightsDock::InsightsDock() {
	// Toolbar.
	toolbar = memnew(HBoxContainer);
	add_child(toolbar);

	btn_start = memnew(Button);
	btn_start->set_text(TTR("Start"));
	btn_start->set_tooltip_text(TTR("Start recording a performance capture."));
	toolbar->add_child(btn_start);
	btn_start->connect(SceneStringName(pressed), callable_mp(this, &InsightsDock::_on_start_pressed));

	btn_stop = memnew(Button);
	btn_stop->set_text(TTR("Stop"));
	btn_stop->set_tooltip_text(TTR("Stop the current recording and save the capture."));
	toolbar->add_child(btn_stop);
	btn_stop->connect(SceneStringName(pressed), callable_mp(this, &InsightsDock::_on_stop_pressed));

	btn_open = memnew(Button);
	btn_open->set_text(TTR("Open"));
	btn_open->set_tooltip_text(TTR("Open an existing .gitracy or .tracy capture file."));
	toolbar->add_child(btn_open);
	btn_open->connect(SceneStringName(pressed), callable_mp(this, &InsightsDock::_on_open_pressed));

	btn_compare = memnew(Button);
	btn_compare->set_text(TTR("Compare"));
	btn_compare->set_tooltip_text(TTR("Compare two capture files to detect regressions."));
	toolbar->add_child(btn_compare);
	btn_compare->connect(SceneStringName(pressed), callable_mp(this, &InsightsDock::_on_compare_pressed));

	btn_clear = memnew(Button);
	btn_clear->set_text(TTR("Clear"));
	btn_clear->set_tooltip_text(TTR("Clear the current capture data from the view."));
	toolbar->add_child(btn_clear);
	btn_clear->connect(SceneStringName(pressed), callable_mp(this, &InsightsDock::_on_clear_pressed));

	// Float button — moves Insights to its own window.
	btn_float = memnew(Button);
	btn_float->set_text(TTR("Float"));
	btn_float->set_tooltip_text(TTR("Make the Insights panel floating."));
	toolbar->add_child(btn_float);
	btn_float->connect(SceneStringName(pressed), callable_mp(this, &InsightsDock::_on_float_pressed));

	// File dialogs.
	open_dialog = memnew(FileDialog);
	open_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	open_dialog->set_title(TTR("Open Insights Capture"));
	open_dialog->add_filter(TTR("*.gitracy ; Godot Insights Capture"));
	open_dialog->add_filter(TTR("*.tracy ; Tracy Capture"));
	open_dialog->connect("file_selected", callable_mp(this, &InsightsDock::_on_open_file_selected));
	add_child(open_dialog);

	compare_baseline_dialog = memnew(FileDialog);
	compare_baseline_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	compare_baseline_dialog->set_title(TTR("Select Baseline Capture"));
	compare_baseline_dialog->add_filter(TTR("*.gitracy ; Godot Insights Capture"));
	compare_baseline_dialog->connect("file_selected", callable_mp(this, &InsightsDock::_on_compare_baseline_selected));
	add_child(compare_baseline_dialog);

	compare_current_dialog = memnew(FileDialog);
	compare_current_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	compare_current_dialog->set_title(TTR("Select Current Capture"));
	compare_current_dialog->add_filter(TTR("*.gitracy ; Godot Insights Capture"));
	compare_current_dialog->connect("file_selected", callable_mp(this, &InsightsDock::_on_compare_current_selected));
	add_child(compare_current_dialog);

	// Channel tabs.
	channel_tabs = memnew(TabContainer);
	channel_tabs->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(channel_tabs);

	// CPU tab: Timeline (filtered to CPU) + Flamegraph.
	{
		VBoxContainer *cpu_vbox = memnew(VBoxContainer);
		cpu_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
		channel_tabs->add_child(cpu_vbox);
		channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("CPU"));

		InsightsTimeline *cpu_timeline = memnew(InsightsTimeline);
		cpu_timeline->set_channel_filter(InsightsTimeline::CHANNEL_CPU);
		cpu_timeline->set_v_size_flags(SIZE_EXPAND_FILL);
		cpu_vbox->add_child(cpu_timeline);
	}

	// GPU tab: Timeline (filtered to GPU) + Flamegraph.
	{
		VBoxContainer *gpu_vbox = memnew(VBoxContainer);
		gpu_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
		channel_tabs->add_child(gpu_vbox);
		channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("GPU"));

		InsightsTimeline *gpu_timeline = memnew(InsightsTimeline);
		gpu_timeline->set_channel_filter(InsightsTimeline::CHANNEL_GPU);
		gpu_timeline->set_v_size_flags(SIZE_EXPAND_FILL);
		gpu_vbox->add_child(gpu_timeline);

		InsightsFlamegraph *gpu_flamegraph = memnew(InsightsFlamegraph);
		gpu_flamegraph->set_v_size_flags(SIZE_EXPAND_FILL);
		gpu_vbox->add_child(gpu_flamegraph);
	}

	// Memory tab: Timeline (filtered to Memory) + MemoryPanel.
	{
		VBoxContainer *mem_vbox = memnew(VBoxContainer);
		mem_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
		channel_tabs->add_child(mem_vbox);
		channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Memory"));

		InsightsTimeline *mem_timeline = memnew(InsightsTimeline);
		mem_timeline->set_channel_filter(InsightsTimeline::CHANNEL_MEMORY);
		mem_timeline->set_custom_minimum_size(Size2(0, 150));
		mem_vbox->add_child(mem_timeline);

		InsightsMemoryPanel *memory_panel = memnew(InsightsMemoryPanel);
		memory_panel->set_v_size_flags(SIZE_EXPAND_FILL);
		mem_vbox->add_child(memory_panel);
	}

	// Loading tab: Timeline (filtered to Loading) + LoadingPanel.
	{
		VBoxContainer *load_vbox = memnew(VBoxContainer);
		load_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
		channel_tabs->add_child(load_vbox);
		channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Loading"));

		InsightsTimeline *load_timeline = memnew(InsightsTimeline);
		load_timeline->set_channel_filter(InsightsTimeline::CHANNEL_LOADING);
		load_timeline->set_custom_minimum_size(Size2(0, 150));
		load_vbox->add_child(load_timeline);

		InsightsLoadingPanel *loading_panel = memnew(InsightsLoadingPanel);
		loading_panel->set_v_size_flags(SIZE_EXPAND_FILL);
		load_vbox->add_child(loading_panel);
	}

	// Network tab: Timeline (filtered to Network) + NetworkPanel.
	{
		VBoxContainer *net_vbox = memnew(VBoxContainer);
		net_vbox->set_v_size_flags(SIZE_EXPAND_FILL);
		channel_tabs->add_child(net_vbox);
		channel_tabs->set_tab_title(channel_tabs->get_tab_count() - 1, TTR("Network"));

		InsightsTimeline *net_timeline = memnew(InsightsTimeline);
		net_timeline->set_channel_filter(InsightsTimeline::CHANNEL_NETWORK);
		net_timeline->set_custom_minimum_size(Size2(0, 150));
		net_vbox->add_child(net_timeline);

		InsightsNetworkPanel *network_panel = memnew(InsightsNetworkPanel);
		network_panel->set_v_size_flags(SIZE_EXPAND_FILL);
		net_vbox->add_child(network_panel);
	}

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

	// Set initial button states.
	_update_button_states();
}

void InsightsDock::_update_button_states() {
	InsightsManager *mgr = InsightsManager::get_singleton();
	bool is_recording = mgr && mgr->is_recording();

	btn_start->set_disabled(is_recording);
	btn_stop->set_disabled(!is_recording);

	if (is_recording) {
		btn_start->set_text(TTR("Recording..."));
	} else {
		btn_start->set_text(TTR("Start"));
	}
}

void InsightsDock::_on_start_pressed() {
	InsightsManager *mgr = InsightsManager::get_singleton();
	if (!mgr) {
		return;
	}

	// Generate a timestamped capture path.
	String timestamp = Time::get_singleton()->get_datetime_string_from_system().replace(":", "-");
	String capture_path = "res://insights_capture_" + timestamp + ".gitracy";

	Error err = mgr->start_capture(capture_path);
	if (err != OK) {
		WARN_PRINT(vformat("Insights: Failed to start capture (error %d).", err));
		return;
	}

	print_line(vformat("Insights: Recording started → %s", capture_path));
	_update_button_states();
}

void InsightsDock::_on_stop_pressed() {
	InsightsManager *mgr = InsightsManager::get_singleton();
	if (!mgr || !mgr->is_recording()) {
		return;
	}

	// Get the database reference before stopping (stop_capture will unref it).
	Ref<InsightsDatabase> db = mgr->get_database();

	String result_path = mgr->stop_capture();
	print_line(vformat("Insights: Recording saved → %s", result_path));

	// Use the database we grabbed before stop_capture released it.
	if (db.is_valid() && db->get_zone_count() > 0) {
		set_database(db);
		print_line(vformat("Insights: Loaded %d zones, %d frames from capture.",
				db->get_zone_count(), db->get_frame_marker_count()));
	} else if (!result_path.is_empty()) {
		// Fallback: load from file if direct db reference failed.
		Ref<InsightsDatabase> file_db;
		file_db.instantiate();
		Error err = file_db->load_from_file(result_path);
		if (err == OK) {
			set_database(file_db);
			print_line(vformat("Insights: Loaded %d zones, %d frames from file.",
					file_db->get_zone_count(), file_db->get_frame_marker_count()));
		} else {
			WARN_PRINT(vformat("Insights: Failed to load capture file: %s", result_path));
		}
	}

	_update_button_states();
}

void InsightsDock::_on_open_pressed() {
	open_dialog->popup_file_dialog();
}

void InsightsDock::_on_open_file_selected(const String &p_path) {
	Ref<InsightsDatabase> db;
	db.instantiate();

	Error err = db->load_from_file(p_path);
	if (err != OK) {
		WARN_PRINT(vformat("Insights: Failed to open capture file: %s", p_path));
		return;
	}

	set_database(db);
	print_line(vformat("Insights: Opened %s — %d zones, %d frames.",
			p_path, db->get_zone_count(), db->get_frame_marker_count()));
}

void InsightsDock::_on_compare_pressed() {
	compare_baseline_path = "";
	compare_baseline_dialog->popup_file_dialog();
}

void InsightsDock::_on_compare_baseline_selected(const String &p_path) {
	compare_baseline_path = p_path;
	// Now ask for the current capture.
	compare_current_dialog->popup_file_dialog();
}

void InsightsDock::_on_compare_current_selected(const String &p_path) {
	if (compare_baseline_path.is_empty()) {
		WARN_PRINT("Insights: No baseline file selected for comparison.");
		return;
	}

	// Load both captures.
	Ref<InsightsDatabase> baseline_db;
	baseline_db.instantiate();
	Error err = baseline_db->load_from_file(compare_baseline_path);
	if (err != OK) {
		WARN_PRINT(vformat("Insights: Failed to load baseline: %s", compare_baseline_path));
		return;
	}

	Ref<InsightsDatabase> current_db;
	current_db.instantiate();
	err = current_db->load_from_file(p_path);
	if (err != OK) {
		WARN_PRINT(vformat("Insights: Failed to load current: %s", p_path));
		return;
	}

	// Set the databases on the compare panel.
	InsightsComparePanel *compare = get_compare_panel();
	if (compare) {
		compare->set_baseline(baseline_db);
		compare->set_current(current_db);
		// Switch to the Compare tab.
		for (int i = 0; i < channel_tabs->get_tab_count(); i++) {
			if (channel_tabs->get_tab_title(i) == TTR("Compare")) {
				channel_tabs->set_current_tab(i);
				break;
			}
		}
	}

	print_line(vformat("Insights: Comparing baseline=%s vs current=%s",
			compare_baseline_path, p_path));
}

void InsightsDock::_on_clear_pressed() {
	current_database.unref();

	// Clear all panels recursively.
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		Control *child = Object::cast_to<Control>(channel_tabs->get_child(i));
		if (child) {
			_set_database_recursive(child, Ref<InsightsDatabase>());
		}
	}

	print_line("Insights: Capture data cleared.");
}

void InsightsDock::_on_float_pressed() {
	EditorDock *dock = Object::cast_to<EditorDock>(get_parent());
	if (dock) {
		EditorDockManager::get_singleton()->make_dock_floating(dock);
	}
}

void InsightsDock::set_database(const Ref<InsightsDatabase> &p_db) {
	current_database = p_db;

	// Set database on all Timeline children (each tab has its own filtered Timeline).
	for (int i = 0; i < channel_tabs->get_child_count(); i++) {
		Control *child = Object::cast_to<Control>(channel_tabs->get_child(i));
		if (child) {
			_set_database_recursive(child, p_db);
		}
	}
}

void InsightsDock::_set_database_recursive(Control *p_control, const Ref<InsightsDatabase> &p_db) {
	InsightsTimeline *timeline = Object::cast_to<InsightsTimeline>(p_control);
	if (timeline) {
		timeline->set_database(p_db);
	}
	InsightsFlamegraph *flamegraph = Object::cast_to<InsightsFlamegraph>(p_control);
	if (flamegraph) {
		flamegraph->set_database(p_db);
	}
	InsightsMemoryPanel *memory = Object::cast_to<InsightsMemoryPanel>(p_control);
	if (memory) {
		memory->set_database(p_db);
	}
	InsightsLoadingPanel *loading = Object::cast_to<InsightsLoadingPanel>(p_control);
	if (loading) {
		loading->set_database(p_db);
	}
	InsightsNetworkPanel *network = Object::cast_to<InsightsNetworkPanel>(p_control);
	if (network) {
		network->set_database(p_db);
	}

	// Recurse into children.
	for (int i = 0; i < p_control->get_child_count(); i++) {
		Control *child = Object::cast_to<Control>(p_control->get_child(i));
		if (child) {
			_set_database_recursive(child, p_db);
		}
	}
}

Ref<InsightsDatabase> InsightsDock::get_database() const {
	return current_database;
}

InsightsTimeline *InsightsDock::get_timeline() const {
	return _find_child_of_type<InsightsTimeline>(channel_tabs);
}

InsightsFlamegraph *InsightsDock::get_flamegraph() const {
	return _find_child_of_type<InsightsFlamegraph>(channel_tabs);
}

InsightsMemoryPanel *InsightsDock::get_memory_panel() const {
	return _find_child_of_type<InsightsMemoryPanel>(channel_tabs);
}

InsightsLoadingPanel *InsightsDock::get_loading_panel() const {
	return _find_child_of_type<InsightsLoadingPanel>(channel_tabs);
}

InsightsNetworkPanel *InsightsDock::get_network_panel() const {
	return _find_child_of_type<InsightsNetworkPanel>(channel_tabs);
}

InsightsComparePanel *InsightsDock::get_compare_panel() const {
	return _find_child_of_type<InsightsComparePanel>(channel_tabs);
}

void InsightsDock::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_database", "db"), &InsightsDock::set_database);
	ClassDB::bind_method(D_METHOD("get_database"), &InsightsDock::get_database);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "database", PROPERTY_HINT_RESOURCE_TYPE, "InsightsDatabase"), "set_database", "get_database");
}

#endif // TOOLS_ENABLED
