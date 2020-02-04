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

	class ScriptStackFrame {
	public:
		String file;
		int line;
		String function;

		ScriptStackFrame() {
			line = -1;
		}
	};

	class ScriptStackDump {
	public:
		List<ScriptStackFrame> frames;
		ScriptStackDump() {}

		void serialize(Array &r_arr);
		void deserialize();
	};

	class Message {

	public:
		String message;
		Array data;

		void serialize(Array &r_arr);
		void deserialize();

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
		Array callstack;

		OutputError() {
			hr = -1;
			min = -1;
			sec = -1;
			msec = -1;
			source_line = -1;
			warning = false;
		}

		void serialize(Array &r_arr);
		void deserialize();
	};

	/*
	class ScriptStackVars {
	public:
		ScriptStackVars() {
		}
	};
	*/

	class ResourceUsage {
	public:
		List<ResourceInfo> infos;

		void serialize(Array &r_arr);
		void deserialize();
	};

	class ProfilerFrame {
	public:

		int frame_number;
		float frame_time;
		float idle_time;
		float physics_time;
		float physics_frame_time;
		// float script_time; // XXX Removed?

		Vector<FrameInfo> frame_data;
		Vector<FrameFunction> frame_functions;

		ProfilerFrame() {
			frame_number = 0;
			frame_time = 0;
			idle_time = 0;
			physics_time = 0;
			physics_frame_time = 0;
		}

		void serialize(Array &r_arr);
		void deserialize();
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

