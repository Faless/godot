#ifndef EDITOR_DEBUGGER_NODE_H
#define EDITOR_DEBUGGER_NODE_H

#include "core/io/tcp_server.h"
#include "editor/debugger/script_editor_debugger.h"
#include "scene/gui/button.h"
#include "scene/gui/tab_container.h"

class EditorDebuggerNode : public TabContainer {

	GDCLASS(EditorDebuggerNode, TabContainer);

private:
	ScriptEditorDebugger *debugger;
	EditorNode *editor;
	Ref<TCP_Server> server;
	Button *debugger_button;

	int last_error_count;
	int last_warning_count;

	ScriptEditorDebugger *_add_debugger(String p_name);

protected:
	void _goto_script_line(REF p_script, int p_line) {
		emit_signal("goto_script_line", p_script, p_line);
	}

	void _set_execution(REF p_script, int p_line) {
		emit_signal("set_execution", p_script, p_line);
	}

	void _clear_execution(REF p_script) {
		emit_signal("clear_execution", p_script);
	}

	void _breaked(bool p_breaked, bool p_can_debug);

	void _show_debugger(bool p_show) {
		emit_signal("show_debugger", p_show);
	}

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	EditorDebuggerNode(EditorNode *p_editor);

	void reload_scripts() {
		debugger->reload_scripts();
	}

	void set_hide_on_stop(bool p_enabled) {
		debugger->set_hide_on_stop(p_enabled);
	}

	void set_debug_with_external_editor(bool p_enabled) {
		debugger->set_debug_with_external_editor(p_enabled);
	}

	void debug_next() {
		debugger->debug_next();
	}

	void debug_step() {
		debugger->debug_step();
	}

	void debug_break() {
		debugger->debug_break();
	}

	void debug_continue() {
		debugger->debug_continue();
	}

	void set_tool_button(Button *p_button) {
		debugger_button = p_button;
	}

	String get_var_value(const String &p_var) const {
		return debugger->get_var_value(p_var);
	}

	Ref<Script> get_dump_stack_script() const {
		return debugger->get_dump_stack_script();
	}

	ScriptEditorDebugger *get_debugger() {
		return debugger;
	}

	bool get_debug_with_external_editor() {
		return debugger->get_debug_with_external_editor();
	}

	Error start();

	void stop();
};

#endif // EDITOR_DEBUGGER_NODE_H
