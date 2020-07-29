/*************************************************************************/
/*  engine_debugger.cpp                                                  */
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

#include "engine_debugger.h"

#include "core/callable.h"
#include "core/debugger/local_debugger.h"
#include "core/debugger/remote_debugger.h"
#include "core/debugger/remote_debugger_peer.h"
#include "core/debugger/script_debugger.h"
#include "core/os/os.h"

EngineDebugger *EngineDebugger::singleton = nullptr;
ScriptDebugger *EngineDebugger::script_debugger = nullptr;

Map<StringName, EngineDebugger::Profiler> EngineDebugger::profilers;
Map<StringName, EngineDebugger::Capture> EngineDebugger::captures;
Map<String, EngineDebugger::CreatePeerFunc> EngineDebugger::protocols;

void EngineDebugger::register_profiler(const StringName &p_name, const Profiler &p_func) {
	ERR_FAIL_COND_MSG(profilers.has(p_name), "Profiler already registered: " + p_name);
	profilers.insert(p_name, p_func);
}

void EngineDebugger::unregister_profiler(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!profilers.has(p_name), "Profiler not registered: " + p_name);
	Profiler &p = profilers[p_name];
	if (p.active) {
		p.toggle(false, Array());
		p.active = false;
	}
	profilers.erase(p_name);
}

void EngineDebugger::register_message_capture(const StringName &p_name, Capture p_func) {
	ERR_FAIL_COND_MSG(captures.has(p_name), "Capture already registered: " + p_name);
	captures.insert(p_name, p_func);
}

void EngineDebugger::unregister_message_capture(const StringName &p_name) {
	ERR_FAIL_COND_MSG(!captures.has(p_name), "Capture not registered: " + p_name);
	captures.erase(p_name);
}

void EngineDebugger::register_uri_handler(const String &p_protocol, CreatePeerFunc p_func) {
	ERR_FAIL_COND_MSG(protocols.has(p_protocol), "Protocol handler already registered: " + p_protocol);
	protocols.insert(p_protocol, p_func);
}

void EngineDebugger::profiler_enable(const StringName &p_name, bool p_enabled, const Array &p_opts) {
	ERR_FAIL_COND_MSG(!profilers.has(p_name), "Can't change profiler state, no profiler: " + p_name);
	Profiler &p = profilers[p_name];
	p.toggle(p_enabled, p_opts);
	p.active = p_enabled;
}

void EngineDebugger::profiler_add_frame_data(const StringName &p_name, const Array &p_data) {
	ERR_FAIL_COND_MSG(!profilers.has(p_name), "Can't add frame data, no profiler: " + p_name);
	Profiler &p = profilers[p_name];
	p.add(p_data);
}

bool EngineDebugger::is_profiling(const StringName &p_name) {
	return profilers.has(p_name) && profilers[p_name].active;
}

bool EngineDebugger::has_profiler(const StringName &p_name) {
	return profilers.has(p_name);
}

bool EngineDebugger::has_capture(const StringName &p_name) {
	return captures.has(p_name);
}

Error EngineDebugger::capture_parse(const StringName &p_name, const String &p_msg, const Array &p_args, bool &r_captured) {
	r_captured = false;
	ERR_FAIL_COND_V_MSG(!captures.has(p_name), ERR_UNCONFIGURED, "Capture not registered: " + p_name);
	const Capture &cap = captures[p_name];
	return cap.capture(p_msg, p_args, r_captured);
}

void EngineDebugger::line_poll() {
	// The purpose of this is just processing events every now and then when the script might get too busy otherwise bugs like infinite loops can't be caught
	if (poll_every % 2048 == 0) {
		poll_events(false);
	}
	poll_every++;
}

void EngineDebugger::iteration(uint64_t p_frame_ticks, uint64_t p_idle_ticks, uint64_t p_physics_ticks, float p_physics_frame_time) {
	frame_time = USEC_TO_SEC(p_frame_ticks);
	idle_time = USEC_TO_SEC(p_idle_ticks);
	physics_time = USEC_TO_SEC(p_physics_ticks);
	physics_frame_time = p_physics_frame_time;
	// Notify tick to running profilers
	for (Map<StringName, Profiler>::Element *E = profilers.front(); E; E = E->next()) {
		Profiler &p = E->get();
		if (!p.active) {
			continue;
		}
		p.tick(frame_time, idle_time, physics_time, physics_frame_time);
	}
	singleton->poll_events(true);
}

void EngineDebugger::initialize(const String &p_uri, bool p_skip_breakpoints, Vector<String> p_breakpoints) {
	register_uri_handler("tcp://", RemoteDebuggerPeerTCP::create); // TCP is the default protocol. Platforms/modules can add more.
	if (p_uri.empty()) {
		return;
	}
	if (p_uri == "local://") {
		singleton = memnew(LocalDebugger);
		script_debugger = memnew(ScriptDebugger);
		// Tell the OS that we want to handle termination signals.
		OS::get_singleton()->initialize_debugging();
	} else if (p_uri.find("://") >= 0) {
		const String proto = p_uri.substr(0, p_uri.find("://") + 3);
		if (!protocols.has(proto)) {
			return;
		}
		RemoteDebuggerPeer *peer = protocols[proto](p_uri);
		if (!peer) {
			return;
		}
		singleton = memnew(RemoteDebugger(Ref<RemoteDebuggerPeer>(peer)));
		script_debugger = memnew(ScriptDebugger);
		// Notify editor of our pid (to allow focus stealing).
		Array msg;
		msg.push_back(OS::get_singleton()->get_process_id());
		singleton->send_message("set_pid", msg);
	}
	if (!singleton) {
		return;
	}

	// There is a debugger, parse breakpoints.
	ScriptDebugger *singleton_script_debugger = singleton->get_script_debugger();
	singleton_script_debugger->set_skip_breakpoints(p_skip_breakpoints);

	for (int i = 0; i < p_breakpoints.size(); i++) {
		String bp = p_breakpoints[i];
		int sp = bp.rfind(":");
		ERR_CONTINUE_MSG(sp == -1, "Invalid breakpoint: '" + bp + "', expected file:line format.");

		singleton_script_debugger->insert_breakpoint(bp.substr(sp + 1, bp.length()).to_int(), bp.substr(0, sp));
	}
}

void EngineDebugger::deinitialize() {
	if (singleton) {
		// Stop all profilers
		for (Map<StringName, Profiler>::Element *E = profilers.front(); E; E = E->next()) {
			if (E->get().active) {
				singleton->profiler_enable(E->key(), false);
			}
		}

		// Flush any remaining message
		singleton->poll_events(false);

		memdelete(singleton);
		singleton = nullptr;
	}

	// Clear profilers/captuers/protocol handlers.
	profilers.clear();
	captures.clear();
	protocols.clear();
}

EngineDebugger::~EngineDebugger() {
	if (script_debugger) {
		memdelete(script_debugger);
	}
	script_debugger = nullptr;
	singleton = nullptr;
}

EngineDebugger::Profiler::Profiler() {
	active = false;
	type = FUNCTION_POINTER;
	function_pointers.toggle = nullptr;
	function_pointers.tick = nullptr;
	function_pointers.add = nullptr;
	function_pointers.data = nullptr;
}

EngineDebugger::Profiler::Profiler(void *p_data, ProfilingToggle p_toggle, ProfilingAdd p_add, ProfilingTick p_tick) {
	active = false;
	type = FUNCTION_POINTER;
	function_pointers.data = p_data;
	function_pointers.toggle = p_toggle;
	function_pointers.add = p_add;
	function_pointers.tick = p_tick;
}

EngineDebugger::Profiler::Profiler(const Callable &p_toggle, const Callable &p_add, const Callable &p_tick) {
	active = false;
	type = CALLABLE;
	memnew_placement(&callables.toggle, Callable(p_toggle));
	memnew_placement(&callables.add, Callable(p_add));
	memnew_placement(&callables.tick, Callable(p_tick));
}

EngineDebugger::Profiler &EngineDebugger::Profiler::operator=(const Profiler &p_profiler) {
	if (type == CALLABLE) {
		callables.toggle.~Callable();
		callables.add.~Callable();
		callables.tick.~Callable();
	}
	type = p_profiler.type;
	active = p_profiler.active;
	switch (type) {
		case CALLABLE: {
			memnew_placement(&callables.toggle, Callable(p_profiler.callables.toggle));
			memnew_placement(&callables.add, Callable(p_profiler.callables.add));
			memnew_placement(&callables.tick, Callable(p_profiler.callables.tick));
		} break;
		case FUNCTION_POINTER: {
			function_pointers = p_profiler.function_pointers;
		}
	}
	return *this;
}

void EngineDebugger::Profiler::toggle(bool p_enable, const Array &p_opts) {
	switch (type) {
		case FUNCTION_POINTER: {
			if (function_pointers.toggle) {
				function_pointers.toggle(function_pointers.data, p_enable, p_opts);
			}
		} break;
		case CALLABLE: {
			if (!callables.toggle.is_null()) {
				Variant arg_enable(p_enable);
				Variant arg_array(p_opts);
				Vector<const Variant *> arguments;
				arguments.resize(2);
				arguments.set(0, &arg_enable);
				arguments.set(1, &arg_array);
				Variant return_value;
				Callable::CallError error;
				callables.toggle.call((const Variant **)arguments.ptr(), arguments.size(), return_value, error);
				ERR_FAIL_COND_MSG(error.error != Callable::CallError::CALL_OK, "Error calling 'toggle' to callable: " + Variant::get_callable_error_text(callables.toggle, (const Variant **)arguments.ptr(), arguments.size(), error));
			}
		}
	}
}

void EngineDebugger::Profiler::tick(float p_frame_time, float p_idle_time, float p_physics_time, float p_physics_frame_time) {
	switch (type) {
		case FUNCTION_POINTER: {
			if (function_pointers.tick) {
				function_pointers.tick(function_pointers.data, p_frame_time, p_idle_time, p_physics_time, p_physics_frame_time);
			}
		} break;
		case CALLABLE: {
			if (!callables.tick.is_null()) {
				Variant arg_frame_time(p_frame_time);
				Variant arg_idle_time(p_idle_time);
				Variant arg_physics_time(p_physics_time);
				Variant arg_physics_frame_time(p_physics_frame_time);
				Vector<const Variant *> arguments;
				arguments.resize(4);
				arguments.set(0, &arg_frame_time);
				arguments.set(1, &arg_idle_time);
				arguments.set(2, &arg_physics_time);
				arguments.set(3, &arg_physics_frame_time);
				Variant return_value;
				Callable::CallError error;
				callables.tick.call((const Variant **)arguments.ptr(), arguments.size(), return_value, error);
				ERR_FAIL_COND_MSG(error.error != Callable::CallError::CALL_OK, "Error calling 'tick' to callable: " + Variant::get_callable_error_text(callables.tick, (const Variant **)arguments.ptr(), arguments.size(), error));
			}
		}
	}
}

void EngineDebugger::Profiler::add(const Array &p_arr) {
	switch (type) {
		case FUNCTION_POINTER: {
			if (function_pointers.add) {
				function_pointers.add(function_pointers.data, p_arr);
			}
		} break;
		case CALLABLE: {
			if (!callables.add.is_null()) {
				Variant arg_arr(p_arr);
				Vector<const Variant *> arguments;
				arguments.push_back(&arg_arr);
				Variant return_value;
				Callable::CallError error;
				callables.add.call((const Variant **)arguments.ptr(), arguments.size(), return_value, error);
				ERR_FAIL_COND_MSG(error.error != Callable::CallError::CALL_OK, "Error calling 'add' to callable: " + Variant::get_callable_error_text(callables.add, (const Variant **)arguments.ptr(), arguments.size(), error));
			}
		}
	}
}

EngineDebugger::Profiler::~Profiler() {
	if (type == CALLABLE) {
		callables.toggle.~Callable();
		callables.add.~Callable();
		callables.tick.~Callable();
	}
}

EngineDebugger::Capture::Capture() {
	type = FUNCTION_POINTER;
	function_pointer.capture = nullptr;
	function_pointer.data = nullptr;
}

EngineDebugger::Capture::Capture(void *p_data, CaptureFunc p_capture_function) {
	type = FUNCTION_POINTER;
	function_pointer.capture = p_capture_function;
	function_pointer.data = p_data;
}

EngineDebugger::Capture::Capture(const Callable &p_callable) {
	type = CALLABLE;
	memnew_placement(&callable, Callable(p_callable));
}

EngineDebugger::Capture::Capture(const Capture &p_capture) {
	type = p_capture.type;
	switch (type) {
		case CALLABLE: {
			memnew_placement(&callable, Callable(p_capture.callable));
		} break;
		case FUNCTION_POINTER: {
			function_pointer = p_capture.function_pointer;
		}
	}
}

EngineDebugger::Capture &EngineDebugger::Capture::operator=(const Capture &p_capture) {
	if (type == CALLABLE) {
		callable.~Callable();
	}
	type = p_capture.type;
	switch (type) {
		case CALLABLE: {
			memnew_placement(&callable, Callable(p_capture.callable));
		} break;
		case FUNCTION_POINTER: {
			function_pointer = p_capture.function_pointer;
		}
	}
	return *this;
}

Error EngineDebugger::Capture::capture(const String &p_msg, const Array &p_args, bool &r_captured) const {
	r_captured = false;
	switch (type) {
		case CALLABLE: {
			Variant message(p_msg);
			Variant args(p_args);
			Vector<const Variant *> arguments;
			arguments.resize(2);
			arguments.set(0, &message);
			arguments.set(1, &args);
			Variant return_value;
			Callable::CallError error;
			callable.call((const Variant **)arguments.ptr(), arguments.size(), return_value, error);
			ERR_FAIL_COND_V_MSG(error.error != Callable::CallError::CALL_OK, Error::FAILED, "Error calling capture to callable: " + Variant::get_callable_error_text(callable, (const Variant **)arguments.ptr(), arguments.size(), error));
			ERR_FAIL_COND_V_MSG(return_value.get_type() != Variant::BOOL, Error::FAILED, "Error calling capture to callable: " + String(callable) + ". Return type is not bool.");
			r_captured = true;
			return Error::OK;
		} break;
		case FUNCTION_POINTER: {
			return function_pointer.capture(function_pointer.data, p_msg, p_args, r_captured);
		}
	}
	return Error::FAILED;
}

EngineDebugger::Capture::~Capture() {
	if (type == CALLABLE) {
		callable.~Callable();
	}
}
