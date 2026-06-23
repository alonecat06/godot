/**************************************************************************/
/*  script_channel.h                                                      */
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

class ScriptChannel : public InsightsChannel {
	GDCLASS(ScriptChannel, InsightsChannel);

public:
	enum Language {
		LANGUAGE_GDSCRIPT = 0,
		LANGUAGE_C_SHARP = 1,
	};

	struct CallRecord {
		String function_name;
		String file;
		int line = 0;
		Language language = LANGUAGE_GDSCRIPT;
		uint64_t start_ns = 0;
		uint64_t end_ns = 0;
		int depth = 0;
	};

	struct GCEvent {
		uint64_t timestamp_ns = 0;
		int generation = 0;
		int objects_collected = 0;
	};

private:
	LocalVector<CallRecord> call_records;
	LocalVector<GCEvent> gc_events;
	int current_depth = 0;
	uint32_t max_records = 50000;

protected:
	static void _bind_methods();

public:
	void enter_function(const String &p_name, const String &p_file, int p_line, Language p_language = LANGUAGE_GDSCRIPT);
	void leave_function();

	void on_gc_event(int p_generation, int p_objects_collected, uint64_t p_timestamp_ns = 0);

	TypedArray<Dictionary> get_call_records() const;
	TypedArray<Dictionary> get_gc_events() const;

	void set_max_records(uint32_t p_max);
	uint32_t get_max_records() const;

	int get_current_depth() const;

	virtual void on_event(const Dictionary &p_event_data) override;
	virtual Dictionary serialize() override;

	ScriptChannel();
	virtual ~ScriptChannel();
};
