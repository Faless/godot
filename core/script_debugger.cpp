#include "core/script_debugger.h"

void ScriptDebugger::ScriptStackDump::serialize(Array &r_arr) {
	r_arr.push_back(frames.size());
	for (int i = 0; i < frames.size(); i++) {
		r_arr.push_back(frames[i].file);
		r_arr.push_back(frames[i].line);
		r_arr.push_back(frames[i].function);
	}
}

void ScriptDebugger::ScriptStackDump::deserialize() {

}

void ScriptDebugger::OutputError::serialize(Array &r_arr) {
	r_arr.push_back(hr);
	r_arr.push_back(min);
	r_arr.push_back(sec);
	r_arr.push_back(msec);
	r_arr.push_back(source_func);
	r_arr.push_back(source_file);
	r_arr.push_back(source_line);
	r_arr.push_back(error);
	r_arr.push_back(error_descr);
	r_arr.push_back(warning);

	r_arr.push_back(callstack.size());
	for (int i = 0; i < callstack.size(); i++) {
		r_arr.push_back(callstack[i]);
	}
}

void ScriptDebugger::Message::serialize(Array &r_arr) {
	r_arr.push_back(message);
	r_arr.push_back(data);
}

void ScriptDebugger::Message::deserialize() {
}

void ScriptDebugger::OutputError::deserialize() {
}

void ScriptDebugger::ResourceUsage::serialize(Array &r_arr) {
	infos.sort();

	r_arr.push_back(infos.size() * 4);
	for (List<ResourceInfo>::Element *E = infos.front(); E; E = E->next()) {
		r_arr.push_back(E->get().path);
		r_arr.push_back(E->get().type);
		r_arr.push_back(E->get().format);
		r_arr.push_back(E->get().vram);
	}
}

void ScriptDebugger::ResourceUsage::deserialize() {
}

void ScriptDebugger::ProfilerFrame::serialize(Array &r_arr) {
	r_arr.push_back(frame_number);
	r_arr.push_back(frame_time);
	r_arr.push_back(idle_time);
	r_arr.push_back(physics_time);
	r_arr.push_back(physics_frame_time);
	// r_arr.push_back(USEC_TO_SEC(total_script_time)); // XXX this seems unused
	// TODO I don't think we need these two.
	r_arr.push_back(frame_data.size());
	r_arr.push_back(frame_functions.size());
	// END TODO
	for (int i = 0; i < frame_data.size(); i++) {
		r_arr.push_back(frame_data[i].name);
		r_arr.push_back(frame_data[i].self_time);
	}
	for (int i = 0; i < frame_functions.size(); i++) {
		r_arr.push_back(frame_functions[i].sig_id);
		r_arr.push_back(frame_functions[i].call_count);
		r_arr.push_back(frame_functions[i].self_time);
		r_arr.push_back(frame_functions[i].total_time);
	}
}

void ScriptDebugger::ProfilerFrame::deserialize() {
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


