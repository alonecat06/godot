/**************************************************************************/
/*  insights_launcher.cpp                                                 */
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

#include "insights_launcher.h"

Error InsightsLauncher::launch_with_insights(const String &p_project_path, int p_port) {
	port = p_port;
	is_running_flag = true;
	// Stub: actual process launch requires OS::execute which is platform-specific.
	return OK;
}

bool InsightsLauncher::is_running() const {
	return is_running_flag;
}

void InsightsLauncher::stop() {
	is_running_flag = false;
	process_id = -1;
}

int InsightsLauncher::get_port() const {
	return port;
}

void InsightsLauncher::_bind_methods() {
	ClassDB::bind_method(D_METHOD("launch_with_insights", "project_path", "port"), &InsightsLauncher::launch_with_insights, DEFVAL(8086));
	ClassDB::bind_method(D_METHOD("is_running"), &InsightsLauncher::is_running);
	ClassDB::bind_method(D_METHOD("stop"), &InsightsLauncher::stop);
	ClassDB::bind_method(D_METHOD("get_port"), &InsightsLauncher::get_port);
}
