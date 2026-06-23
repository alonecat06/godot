/**************************************************************************/
/*  log_channel.h                                                         */
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

#include "modules/insights/channels/insights_channel.h"
#include "core/templates/local_vector.h"

class LogChannel : public InsightsChannel {
	GDCLASS(LogChannel, InsightsChannel);

public:
	enum Severity {
		SEVERITY_VERBOSE = 0,
		SEVERITY_DEBUG = 1,
		SEVERITY_INFO = 2,
		SEVERITY_WARNING = 3,
		SEVERITY_ERROR = 4,
	};

	struct LogEntry {
		Severity severity = SEVERITY_INFO;
		String text;
		String file;
		int line = 0;
		uint64_t timestamp_ns = 0;
		int zone_id = -1;
	};

private:
	LocalVector<LogEntry> entries;
	Severity severity_threshold = SEVERITY_VERBOSE;
	uint32_t max_entries = 10000;

protected:
	static void _bind_methods();

public:
	void log_message(Severity p_severity, const String &p_text, const String &p_file, int p_line, uint64_t p_timestamp_ns, int p_zone_id = -1);

	void set_severity_threshold(Severity p_threshold);
	Severity get_severity_threshold() const;

	void set_max_entries(uint32_t p_max);
	uint32_t get_max_entries() const;

	TypedArray<Dictionary> get_messages_in_range(uint64_t p_start_ns, uint64_t p_end_ns, Severity p_min_severity = SEVERITY_VERBOSE) const;
	uint32_t get_entry_count() const;

	virtual void on_event(const Dictionary &p_event_data) override;
	virtual Dictionary serialize() override;

	LogChannel();
	virtual ~LogChannel();
};
