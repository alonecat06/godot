/**************************************************************************/
/*  insights_editor_plugin.cpp                                            */
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

#include "modules/insights/editor/insights_editor_plugin.h"

#ifdef TOOLS_ENABLED

#include "modules/insights/editor/insights_dock.h"
#include "modules/insights/insights_core/insights_manager.h"

InsightsEditorPlugin::InsightsEditorPlugin() {
	bottom_dock = memnew(InsightsDock);
	add_control_to_bottom_panel(bottom_dock, TTR("Insights"));
}

bool InsightsEditorPlugin::has_main_screen() const {
	return true;
}

String InsightsEditorPlugin::get_plugin_name() const {
	return TTR("Insights");
}

void InsightsEditorPlugin::make_visible(bool p_visible) {
	if (bottom_dock) {
		if (p_visible) {
			bottom_dock->show();
		} else {
			bottom_dock->hide();
		}
	}
}

InsightsDock *InsightsEditorPlugin::get_bottom_dock() const {
	return bottom_dock;
}

void InsightsEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bottom_dock"), &InsightsEditorPlugin::get_bottom_dock);
}

#endif // TOOLS_ENABLED
