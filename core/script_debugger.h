/*************************************************************************/
/*  script_debugger.h                                                    */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef SCRIPT_DEBUGGER_H
#define SCRIPT_DEBUGGER_H

#include "core/io/multiplayer_api.h"
#include "core/map.h"
#include "core/script_language.h"
#include "core/vector.h"

class ScriptDebugger {

public:
	class ResourceInfo {
	public:
		String path;
		String format;
		String type;
		RID id;
		int vram;
		bool operator<(const ResourceInfo &p_img) const { return vram == p_img.vram ? id < p_img.id : vram > p_img.vram; }
		ResourceInfo() {
			vram = 0;
		}
	};

	class ResourceUsage {
	public:
		List<ResourceInfo> infos;

		Array serialize();
		bool deserialize(Array p_arr);
	};

	class FrameInfo {
	public:
		StringName name;
		float self_time;
		float total_time;

		FrameInfo() {
			self_time = 0;
			total_time = 0;
		}
	};

	class FrameFunction {
	public:
		int sig_id;
		int call_count;
		StringName name;
		float self_time;
		float total_time;

		FrameFunction() {
			sig_id = -1;
			call_count = 0;
			self_time = 0;
			total_time = 0;
		}
	};

	class ScriptStackVariable {
	public:
		String name;
		Variant value;
		int type;
		ScriptStackVariable() {
			type = -1;
		}

		Array serialize(int max_size = 1 << 20); // 1 MiB default.
		bool deserialize(Array p_arr);
	};

	class ScriptStackDump {
	public:
		List<ScriptLanguage::StackInfo> frames;
		ScriptStackDump() {}

		Array serialize();
		bool deserialize(Array p_arr);
	};

	class Message {

	public:
		String message;
		Array data;

		Message() {}
	};

	class OutputError {
	public:
		int hr;
		int min;
		int sec;
		int msec;
		String source_file;
		String source_func;
		int source_line;
		String error;
		String error_descr;
		bool warning;
		Vector<ScriptLanguage::StackInfo> callstack;

		OutputError() {
			hr = -1;
			min = -1;
			sec = -1;
			msec = -1;
			source_line = -1;
			warning = false;
		}

		Array serialize();
		bool deserialize(Array p_data);
	};

	struct FrameData {

		StringName name;
		Array data;
	};

	class ProfilerSignature {
	public:
		StringName name;
		int id;

		Array serialize();
		bool deserialize(Array p_arr);

		ProfilerSignature() {
			id = -1;
		};
	};

	class ProfilerFrame {
	public:
		int frame_number;
		float frame_time;
		float idle_time;
		float physics_time;
		float physics_frame_time;
		float script_time;

		Vector<FrameData> frames_data;
		Vector<FrameFunction> frame_functions;

		ProfilerFrame() {
			frame_number = 0;
			frame_time = 0;
			idle_time = 0;
			physics_time = 0;
			physics_frame_time = 0;
		}

		Array serialize();
		bool deserialize(Array p_arr);
	};

	class NetworkProfilerFrame {
	public:
		Vector<MultiplayerAPI::ProfilingInfo> infos;

		Array serialize();
		bool deserialize(Array p_arr);

		NetworkProfilerFrame(){};
	};

protected:
	int lines_left;
	int depth;

	static ScriptDebugger *singleton;
	Map<int, Set<StringName> > breakpoints;

	ScriptLanguage *break_lang;

public:
	_FORCE_INLINE_ static ScriptDebugger *get_singleton() { return singleton; }
	void set_lines_left(int p_left);
	int get_lines_left() const;

	void set_depth(int p_depth);
	int get_depth() const;

	String breakpoint_find_source(const String &p_source) const;
	void insert_breakpoint(int p_line, const StringName &p_source);
	void remove_breakpoint(int p_line, const StringName &p_source);
	bool is_breakpoint(int p_line, const StringName &p_source) const;
	bool is_breakpoint_line(int p_line) const;
	void clear_breakpoints();
	const Map<int, Set<StringName> > &get_breakpoints() const { return breakpoints; }

	virtual void debug(ScriptLanguage *p_script, bool p_can_continue = true, bool p_is_error_breakpoint = false) = 0;
	virtual void idle_poll();
	virtual void line_poll();

	void set_break_language(ScriptLanguage *p_lang);
	ScriptLanguage *get_break_language() const;

	virtual void send_message(const String &p_message, const Array &p_args) = 0;
	virtual void send_error(const String &p_func, const String &p_file, int p_line, const String &p_err, const String &p_descr, ErrorHandlerType p_type, const Vector<ScriptLanguage::StackInfo> &p_stack_info) = 0;

	virtual bool is_remote() const { return false; }
	virtual void request_quit() {}

	virtual void set_multiplayer(Ref<MultiplayerAPI> p_multiplayer) {}

	virtual bool is_profiling() const = 0;
	virtual void add_profiling_frame_data(const StringName &p_name, const Array &p_data) = 0;
	virtual void profiling_start() = 0;
	virtual void profiling_end() = 0;
	virtual void profiling_set_frame_times(float p_frame_time, float p_idle_time, float p_physics_time, float p_physics_frame_time) = 0;

	ScriptDebugger();
	virtual ~ScriptDebugger() { singleton = NULL; }
};

#endif // SCRIPT_DEBUGGER_H
