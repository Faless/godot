/*************************************************************************/
/*  engine_debugger.h                                                    */
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

#ifndef ENGINE_DEBUGGER_H
#define ENGINE_DEBUGGER_H

#include "core/array.h"
#include "core/map.h"
#include "core/string_name.h"
#include "core/ustring.h"
#include "core/variant.h"
#include "core/vector.h"

class RemoteDebuggerPeer;
class ScriptDebugger;
class ScriptEditorDebugger;

class EngineDebugger {
public:
	typedef void (*ProfilingToggle)(void *p_user, bool p_enable, const Array &p_opts);
	typedef void (*ProfilingTick)(void *p_user, float p_frame_time, float p_idle_time, float p_physics_time, float p_physics_frame_time);
	typedef void (*ProfilingAdd)(void *p_user, const Array &p_arr);

	typedef Error (*CaptureFunc)(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured);

	typedef RemoteDebuggerPeer *(*CreatePeerFunc)(const String &p_uri);

	class Profiler {
		friend class EngineDebugger;

		ProfilingToggle toggle = nullptr;
		ProfilingAdd add = nullptr;
		ProfilingTick tick = nullptr;
		void *data = nullptr;
		bool active = false;

	public:
		Profiler() {}
		Profiler(void *p_data, ProfilingToggle p_toggle, ProfilingAdd p_add, ProfilingTick p_tick) {
			data = p_data;
			toggle = p_toggle;
			add = p_add;
			tick = p_tick;
		}
	};

	class Capture {
		friend class EngineDebugger;
		friend class ScriptEditorDebugger;

		CaptureFunc capture = nullptr;
		void *data = nullptr;

	public:
		Capture() {}
		Capture(void *p_data, CaptureFunc p_capture) {
			data = p_data;
			capture = p_capture;
		}
	};

private:
	float frame_time = 0.0;
	float idle_time = 0.0;
	float physics_time = 0.0;
	float physics_frame_time = 0.0;

	uint32_t poll_every = 0;

protected:
	static EngineDebugger *singleton;
	static ScriptDebugger *script_debugger;

	static Map<StringName, Profiler> profilers;
	static Map<StringName, Capture> captures;
	static Map<String, CreatePeerFunc> protocols;

public:
	_FORCE_INLINE_ static EngineDebugger *get_singleton() { return singleton; }
	_FORCE_INLINE_ static bool is_active() { return singleton != nullptr && script_debugger != nullptr; }

	_FORCE_INLINE_ static ScriptDebugger *get_script_debugger() { return script_debugger; };

	static void initialize(const String &p_uri, bool p_skip_breakpoints, Vector<String> p_breakpoints);
	static void deinitialize();
	static void register_profiler(const StringName &p_name, const Profiler &p_profiler);
	static void unregister_profiler(const StringName &p_name);
	static bool is_profiling(const StringName &p_name);
	static bool has_profiler(const StringName &p_name);
	static void profiler_add_frame_data(const StringName &p_name, const Array &p_data);

	static void register_message_capture(const StringName &p_name, Capture p_func);
	static void unregister_message_capture(const StringName &p_name);
	static bool has_capture(const StringName &p_name);

	static void register_uri_handler(const String &p_protocol, CreatePeerFunc p_func);

	void iteration(uint64_t p_frame_ticks, uint64_t p_idle_ticks, uint64_t p_physics_ticks, float p_physics_frame_time);
	void profiler_enable(const StringName &p_name, bool p_enabled, const Array &p_opts = Array());
	Error capture_parse(const StringName &p_name, const String &p_msg, const Array &p_args, bool &r_captured);

	void line_poll();

	virtual void poll_events(bool p_is_idle) {}
	virtual void send_message(const String &p_msg, const Array &p_data) = 0;
	virtual void send_error(const String &p_func, const String &p_file, int p_line, const String &p_err, const String &p_descr, ErrorHandlerType p_type) = 0;
	virtual void debug(bool p_can_continue = true, bool p_is_error_breakpoint = false) = 0;

	virtual ~EngineDebugger();
};

#endif // ENGINE_DEBUGGER_H
