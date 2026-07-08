/**************************************************************************/
/*  insights_dock.h                                                       */
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

#ifdef TOOLS_ENABLED

#include "scene/gui/box_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/button.h"
#include "scene/gui/file_dialog.h"
#include "modules/insights/insights_core/insights_database.h"
#include "editor/docks/editor_dock.h"

#ifdef TRACY_SERVER_ENABLED
#include "modules/insights/insights_tracy_bridge.h"
#endif

class Timer;
class InsightsTimeline;
class InsightsFlamegraph;
class InsightsMemoryPanel;
class InsightsLoadingPanel;
class InsightsNetworkPanel;
class InsightsComparePanel;

class InsightsDock : public VBoxContainer {
	GDCLASS(InsightsDock, VBoxContainer);

private:
	HBoxContainer *toolbar = nullptr;
	TabContainer *channel_tabs = nullptr;
	HBoxContainer *playback_bar = nullptr;
	Button *btn_start = nullptr;
	Button *btn_stop = nullptr;
	Button *btn_open = nullptr;
	Button *btn_compare = nullptr;
	Button *btn_clear = nullptr;
	Button *btn_float = nullptr;
	Ref<InsightsDatabase> current_database;
	FileDialog *open_dialog = nullptr;
	FileDialog *compare_baseline_dialog = nullptr;
	FileDialog *compare_current_dialog = nullptr;
	String compare_baseline_path;

#ifdef TRACY_SERVER_ENABLED
	Ref<InsightsTracyBridge> tracy_bridge;
	Timer *tracy_refresh_timer = nullptr;
	Ref<InsightsDatabase> tracy_live_db;
	Button *btn_save = nullptr;
	FileDialog *save_dialog = nullptr;
	void _on_tracy_refresh_timeout();
	void _on_save_pressed();
	void _on_save_file_selected(const String &p_path);
#endif

	void _update_button_states();
	void _set_database_recursive(Control *p_control, const Ref<InsightsDatabase> &p_db);

	template <typename T>
	T *_find_child_of_type(Control *p_parent) const {
		for (int i = 0; i < p_parent->get_child_count(); i++) {
			T *found = Object::cast_to<T>(p_parent->get_child(i));
			if (found) {
				return found;
			}
			Control *child_ctrl = Object::cast_to<Control>(p_parent->get_child(i));
			if (child_ctrl) {
				T *result = _find_child_of_type<T>(child_ctrl);
				if (result) {
					return result;
				}
			}
		}
		return nullptr;
	}

	void _on_start_pressed();
	void _on_stop_pressed();
	void _on_open_pressed();
#ifdef TRACY_SERVER_ENABLED
	void _on_tracy_save_pressed();
#endif
	void _on_open_file_selected(const String &p_path);
	void _on_compare_pressed();
	void _on_compare_baseline_selected(const String &p_path);
	void _on_compare_current_selected(const String &p_path);
	void _on_clear_pressed();

protected:
	static void _bind_methods();

public:
	void _on_float_pressed();
	void set_database(const Ref<InsightsDatabase> &p_db);
	Ref<InsightsDatabase> get_database() const;
	InsightsTimeline *get_timeline() const;
	InsightsFlamegraph *get_flamegraph() const;
	InsightsMemoryPanel *get_memory_panel() const;
	InsightsLoadingPanel *get_loading_panel() const;
	InsightsNetworkPanel *get_network_panel() const;
	InsightsComparePanel *get_compare_panel() const;

	InsightsDock();
};

#endif // TOOLS_ENABLED
