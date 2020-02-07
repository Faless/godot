/*************************************************************************/
/*  script_debugger.cpp                                                  */
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

#include "script_debugger.h"

#include "core/io/marshalls.h"

#define CHECK_SIZE(arr, expected, what) ERR_FAIL_COND_V_MSG((uint32_t)arr.size() < (uint32_t)(expected), false, String("Malformed ") + what + " message from script debugger, message too short. Exptected size: " + itos(expected) + ", actual size: " + itos(arr.size()))

Array ScriptDebugger::ScriptStackDump::serialize() {
	Array arr;
	arr.push_back(frames.size());
	for (int i = 0; i < frames.size(); i++) {
		arr.push_back(frames[i].file);
		arr.push_back(frames[i].line);
		arr.push_back(frames[i].func);
	}
	return arr;
}

bool ScriptDebugger::ScriptStackDump::deserialize(Array p_arr) {
	CHECK_SIZE(p_arr, 1, "ScriptStackDump");
	uint32_t size = p_arr.pop_front();
	CHECK_SIZE(p_arr, size * 3, "ScriptStackDump");
	for (uint32_t i = 0; i < size; i++) {
		ScriptLanguage::StackInfo sf;
		sf.file = p_arr.pop_front();
		sf.line = p_arr.pop_front();
		sf.func = p_arr.pop_front();
		frames.push_back(sf);
	}
	return true;
}

Array ScriptDebugger::ScriptStackVariable::serialize(int max_size) {
	Array arr;
	arr.push_back(name);
	arr.push_back(type);

	Variant var = value;
	if (value.get_type() == Variant::OBJECT && !ObjectDB::instance_validate(value)) {
		var = Variant();
	}

	int len = 0;
	Error err = encode_variant(var, NULL, len, true);
	if (err != OK)
		ERR_PRINT("Failed to encode variant.");

	if (len > max_size) {
		arr.push_back(Variant());
	} else {
		arr.push_back(var);
	}
	return arr;
}

bool ScriptDebugger::ScriptStackVariable::deserialize(Array p_arr) {
	CHECK_SIZE(p_arr, 3, "ScriptStackVariable");
	name = p_arr.pop_front();
	type = p_arr.pop_front();
	value = p_arr.pop_front();
	return true;
}

Array ScriptDebugger::OutputError::serialize() {
	Array arr;
	arr.push_back(hr);
	arr.push_back(min);
	arr.push_back(sec);
	arr.push_back(msec);
	arr.push_back(source_file);
	arr.push_back(source_func);
	arr.push_back(source_line);
	arr.push_back(error);
	arr.push_back(error_descr);
	arr.push_back(warning);
	unsigned int size = callstack.size();
	const ScriptLanguage::StackInfo *r = callstack.ptr();
	arr.push_back(size);
	for (int i = 0; i < callstack.size(); i++) {
		arr.push_back(r[i].file);
		arr.push_back(r[i].func);
		arr.push_back(r[i].line);
	}
	return arr;
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

Array ScriptDebugger::ResourceUsage::serialize() {
	infos.sort();

	Array arr;
	arr.push_back(infos.size());
	for (List<ResourceInfo>::Element *E = infos.front(); E; E = E->next()) {
		arr.push_back(E->get().path);
		arr.push_back(E->get().format);
		arr.push_back(E->get().type);
		arr.push_back(E->get().vram);
	}
	return arr;
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

Array ScriptDebugger::ProfilerSignature::serialize() {
	Array arr;
	arr.push_back(name);
	arr.push_back(id);
	return arr;
}

bool ScriptDebugger::ProfilerSignature::deserialize(Array p_arr) {
	CHECK_SIZE(p_arr, 2, "ProfilerSignature");
	name = p_arr.pop_front();
	id = p_arr.pop_front();
	return true;
}

Array ScriptDebugger::ProfilerFrame::serialize() {
	Array arr;
	arr.push_back(frame_number);
	arr.push_back(frame_time);
	arr.push_back(idle_time);
	arr.push_back(physics_time);
	arr.push_back(physics_frame_time);
	arr.push_back(USEC_TO_SEC(script_time));

	arr.push_back(frames_data.size());
	arr.push_back(frame_functions.size());

	// Servers profiling info. XXX Awful to parse.
	for (int i = 0; i < frames_data.size(); i++) {
		// TODO how shitty is this?!? (Kept as reference and commenting)
		arr.push_back(frames_data[i].name); // Type (physics/process/audio/...)
		arr.push_back(frames_data[i].data.size());
		for (int j = 0; j < frames_data[i].data.size() / 2; j++) {
			arr.push_back(frames_data[i].data[2 * j]); // NAME
			arr.push_back(frames_data[i].data[2 * j + 1]); // TIME
		}
	}
	for (int i = 0; i < frame_functions.size(); i++) {
		arr.push_back(frame_functions[i].sig_id);
		arr.push_back(frame_functions[i].call_count);
		arr.push_back(frame_functions[i].self_time);
		arr.push_back(frame_functions[i].total_time);
	}
	return arr;
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

Array ScriptDebugger::NetworkProfilerFrame::serialize() {
	Array arr;
	arr.push_back(infos.size());
	for (int i = 0; i < infos.size(); ++i) {
		arr.push_back(infos[i].node);
		arr.push_back(infos[i].node_path);
		arr.push_back(infos[i].incoming_rpc);
		arr.push_back(infos[i].incoming_rset);
		arr.push_back(infos[i].outgoing_rpc);
		arr.push_back(infos[i].outgoing_rset);
	}
	return arr;
}

bool ScriptDebugger::NetworkProfilerFrame::deserialize(Array p_arr) {
	CHECK_SIZE(p_arr, 1, "NetworkProfilerFrame");
	uint32_t size = p_arr.pop_front();
	CHECK_SIZE(p_arr, size * 6, "NetworkProfilerFrame");
	infos.resize(size);
	for (uint32_t i = 0; i < size; ++i) {
		infos.write[i].node = p_arr.pop_front();
		infos.write[i].node_path = p_arr.pop_front();
		infos.write[i].incoming_rpc = p_arr.pop_front();
		infos.write[i].incoming_rset = p_arr.pop_front();
		infos.write[i].outgoing_rpc = p_arr.pop_front();
		infos.write[i].outgoing_rset = p_arr.pop_front();
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
