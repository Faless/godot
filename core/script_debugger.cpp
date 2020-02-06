#include "core/script_debugger.h"

#define CHECK_SIZE(arr, expected, what) ERR_FAIL_COND_V_MSG((uint32_t)arr.size() < (uint32_t)(expected), false, String("Malformed ") + what + " message from script debugger, message too short. Exptected size: " + itos(expected) + ", actual size: " + itos(arr.size()))

void ScriptDebugger::ScriptStackDump::serialize(Array &r_arr) {
	r_arr.push_back(frames.size());
	for (int i = 0; i < frames.size(); i++) {
		r_arr.push_back(frames[i].file);
		r_arr.push_back(frames[i].line);
		r_arr.push_back(frames[i].func);
	}
}

void ScriptDebugger::ScriptStackDump::deserialize() {
}

void ScriptDebugger::OutputError::serialize(Array &r_arr) {
	r_arr.push_back(hr);
	r_arr.push_back(min);
	r_arr.push_back(sec);
	r_arr.push_back(msec);
	r_arr.push_back(source_file);
	r_arr.push_back(source_func);
	r_arr.push_back(source_line);
	r_arr.push_back(error);
	r_arr.push_back(error_descr);
	r_arr.push_back(warning);
	unsigned int size = callstack.size();
	const ScriptLanguage::StackInfo *r = callstack.ptr();
	r_arr.push_back(size);
	for (int i = 0; i < callstack.size(); i++) {
		r_arr.push_back(r[i].file);
		r_arr.push_back(r[i].func);
		r_arr.push_back(r[i].line);
	}
}

bool ScriptDebugger::OutputError::deserialize(Array p_data) {
	ERR_FAIL_COND_V_MSG(p_data.size() < 11, false, "Malformed error message from script debugger. Received size: " + itos(p_data.size()));
	hr = p_data.pop_front();
	min = p_data.pop_front();
	sec = p_data.pop_front();
	msec = p_data.pop_front();
	source_file = p_data.pop_front();
	source_func = p_data.pop_front();
	source_line = p_data.pop_front();
	error = p_data.pop_front();
	error_descr = p_data.pop_front();
	warning = p_data.pop_front();
	unsigned int stack_size = p_data.pop_front();
	ERR_FAIL_COND_V_MSG((unsigned int)p_data.size() < 3 * stack_size, false, "Malformed error message from script debugger, message too short. Exptected size: " + itos(stack_size * 3) + ", actual size: " + itos(p_data.size()));
	callstack.resize(stack_size);
	ScriptLanguage::StackInfo *w = callstack.ptrw();
	for (unsigned int i = 0; i < stack_size; i++) {
		w[i].file = p_data.pop_front();
		w[i].func = p_data.pop_front();
		w[i].line = p_data.pop_front();
	}
	ERR_FAIL_COND_V_MSG(p_data.size() > 0, false, "Malformed error message from script debugger, message too long. Size left after parsing: " + itos(p_data.size()));
	return true;
}

void ScriptDebugger::Message::serialize(Array &r_arr) {
	r_arr.push_back(message);
	r_arr.push_back(data);
}

void ScriptDebugger::Message::deserialize() {
}

void ScriptDebugger::ResourceUsage::serialize(Array &r_arr) {
	infos.sort();

	r_arr.push_back(infos.size());
	for (List<ResourceInfo>::Element *E = infos.front(); E; E = E->next()) {
		r_arr.push_back(E->get().path);
		r_arr.push_back(E->get().format);
		r_arr.push_back(E->get().type);
		r_arr.push_back(E->get().vram);
	}
}

bool ScriptDebugger::ResourceUsage::deserialize(Array p_arr) {
	CHECK_SIZE(p_arr, 1, "ResourceUsage");
	uint32_t size = p_arr.pop_front();
	CHECK_SIZE(p_arr, size * 4, "ResourceUsage");
	for (uint32_t i = 0; i < size; i++) {
		ResourceInfo info;
		info.path = p_arr.pop_front();
		info.format = p_arr.pop_front();
		info.type = p_arr.pop_front();
		info.vram = p_arr.pop_front();
		infos.push_back(info);
	}
	return true;
}

void ScriptDebugger::ProfilerFrame::serialize(Array &r_arr) {
	r_arr.push_back(frame_number);
	r_arr.push_back(frame_time);
	r_arr.push_back(idle_time);
	r_arr.push_back(physics_time);
	r_arr.push_back(physics_frame_time);
	r_arr.push_back(USEC_TO_SEC(script_time));

	r_arr.push_back(frames_data.size());
	r_arr.push_back(frame_functions.size());

	// Servers profiling info. XXX Awful to parse.
	for (int i = 0; i < frames_data.size(); i++) {
		// TODO how shitty is this?!? (Kept as reference and commenting)
		r_arr.push_back(frames_data[i].name); // Type (physics/process/audio/...)
		r_arr.push_back(frames_data[i].data.size());
		for (int j = 0; j < frames_data[i].data.size() / 2; j++) {
			r_arr.push_back(frames_data[i].data[2 * j]); // NAME
			r_arr.push_back(frames_data[i].data[2 * j + 1]); // TIME
		}
	}
	for (int i = 0; i < frame_functions.size(); i++) {
		r_arr.push_back(frame_functions[i].sig_id);
		r_arr.push_back(frame_functions[i].call_count);
		r_arr.push_back(frame_functions[i].self_time);
		r_arr.push_back(frame_functions[i].total_time);
	}
}

bool ScriptDebugger::ProfilerFrame::deserialize(Array p_arr) {
	CHECK_SIZE(p_arr, 8, "ProfilerFrame");
	frame_number = p_arr.pop_front();
	frame_time = p_arr.pop_front();
	idle_time = p_arr.pop_front();
	physics_time = p_arr.pop_front();
	physics_frame_time = p_arr.pop_front();
	script_time = p_arr.pop_front();
	uint32_t frame_data_size = p_arr.pop_front();
	int frame_func_size = p_arr.pop_front();
	while (frame_data_size) {
		CHECK_SIZE(p_arr, 2, "ProfilerFrame");
		frame_data_size--;
		FrameData fd;
		fd.name = p_arr.pop_front();
		int sub_data_size = p_arr.pop_front();
		CHECK_SIZE(p_arr, sub_data_size, "ProfilerFrame");
		// TODO XXX see above.
		for (int j = 0; j < sub_data_size / 2; j++) {
			fd.data.push_back(p_arr.pop_front()); // NAME
			fd.data.push_back(p_arr.pop_front()); // TIME
		}
		frames_data.push_back(fd);
	}
	CHECK_SIZE(p_arr, frame_func_size * 4, "ProfilerFrame");
	for (int i = 0; i < frame_func_size; i++) {
		FrameFunction ff;
		ff.sig_id = p_arr.pop_front();
		ff.call_count = p_arr.pop_front();
		ff.self_time = p_arr.pop_front();
		ff.total_time = p_arr.pop_front();
		frame_functions.push_back(ff);
	}
	return true;
}

ScriptDebugger *ScriptDebugger::singleton = NULL;

void ScriptDebugger::set_lines_left(int p_left) {

	lines_left = p_left;
}

int ScriptDebugger::get_lines_left() const {

	return lines_left;
}

void ScriptDebugger::set_depth(int p_depth) {

	depth = p_depth;
}

int ScriptDebugger::get_depth() const {

	return depth;
}

void ScriptDebugger::insert_breakpoint(int p_line, const StringName &p_source) {

	if (!breakpoints.has(p_line))
		breakpoints[p_line] = Set<StringName>();
	breakpoints[p_line].insert(p_source);
}

void ScriptDebugger::remove_breakpoint(int p_line, const StringName &p_source) {

	if (!breakpoints.has(p_line))
		return;

	breakpoints[p_line].erase(p_source);
	if (breakpoints[p_line].size() == 0)
		breakpoints.erase(p_line);
}
bool ScriptDebugger::is_breakpoint(int p_line, const StringName &p_source) const {

	if (!breakpoints.has(p_line))
		return false;
	return breakpoints[p_line].has(p_source);
}
bool ScriptDebugger::is_breakpoint_line(int p_line) const {

	return breakpoints.has(p_line);
}

String ScriptDebugger::breakpoint_find_source(const String &p_source) const {

	return p_source;
}

void ScriptDebugger::clear_breakpoints() {

	breakpoints.clear();
}

void ScriptDebugger::idle_poll() {
}

void ScriptDebugger::line_poll() {
}

void ScriptDebugger::set_break_language(ScriptLanguage *p_lang) {

	break_lang = p_lang;
}

ScriptLanguage *ScriptDebugger::get_break_language() const {

	return break_lang;
}

ScriptDebugger::ScriptDebugger() {

	singleton = this;
	lines_left = -1;
	depth = -1;
	break_lang = NULL;
}
