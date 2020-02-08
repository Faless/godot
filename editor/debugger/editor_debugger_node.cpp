#include "editor_debugger_node.h"

#include "editor/editor_log.h"
#include "editor/editor_node.h"

EditorDebuggerNode::EditorDebuggerNode(EditorNode *p_editor) {
	editor = p_editor;
	server.instance();
	debugger = _add_debugger("Debugger");
}

ScriptEditorDebugger *EditorDebuggerNode::_add_debugger(String p_name) {
	ScriptEditorDebugger *node = memnew(ScriptEditorDebugger(editor));
	node->set_name(p_name);
	node->connect("goto_script_line", this, "_goto_script_line");
	node->connect("set_execution", this, "_set_execution");
	node->connect("clear_execution", this, "_clear_execution");
	node->connect("breaked", this, "_breaked");
	node->connect("show_debugger", this, "_show_debugger");
	add_child(node);
	return node;
}

void EditorDebuggerNode::_bind_methods() {
	ClassDB::bind_method("_goto_script_line", &EditorDebuggerNode::_goto_script_line);
	ClassDB::bind_method("_set_execution", &EditorDebuggerNode::_set_execution);
	ClassDB::bind_method("_clear_execution", &EditorDebuggerNode::_clear_execution);
	ClassDB::bind_method("_breaked", &EditorDebuggerNode::_breaked);
	ClassDB::bind_method("_show_debugger", &EditorDebuggerNode::_show_debugger);

	ADD_SIGNAL(MethodInfo("goto_script_line"));
	ADD_SIGNAL(MethodInfo("set_execution", PropertyInfo("script"), PropertyInfo(Variant::INT, "line")));
	ADD_SIGNAL(MethodInfo("clear_execution", PropertyInfo("script")));
	ADD_SIGNAL(MethodInfo("breaked", PropertyInfo(Variant::BOOL, "reallydid"), PropertyInfo(Variant::BOOL, "can_debug")));
	ADD_SIGNAL(MethodInfo("show_debugger", PropertyInfo(Variant::BOOL, "reallydid")));
}

Error EditorDebuggerNode::start() {
	if (is_visible_in_tree()) {
		EditorNode::get_singleton()->make_bottom_panel_item_visible(this);
	}
	int remote_port = (int)EditorSettings::get_singleton()->get("network/debug/remote_port");
	const Error err = server->listen(remote_port);
	if (err != OK) {
		EditorNode::get_log()->add_message(String("Error listening on port ") + itos(remote_port), EditorLog::MSG_TYPE_ERROR);
		return err;
	}
	set_process(true);
	return OK;
}

void EditorDebuggerNode::stop() {
	server->stop();
	//debugger->stop();
	// Also close all debugging sessions.
	for (int i = 0; i < get_tab_count(); i++) {
		Object::cast_to<ScriptEditorDebugger>(get_tab_control(i))->stop();
	}
}

void EditorDebuggerNode::_notification(int p_what) {
	if (!server->is_listening())
		return;

	if (p_what != NOTIFICATION_PROCESS)
		return;
	if (server->is_connection_available()) {

		int error_count = 0;
		int warning_count = 0;
		for (int i = 0; i < get_tab_count(); i++) {
			ScriptEditorDebugger *n = Object::cast_to<ScriptEditorDebugger>(get_tab_control(i));
			if (!n)
				continue;
			error_count += n->get_error_count();
			warning_count += n->get_warning_count();
		}

		if (error_count != last_error_count || warning_count != last_warning_count) {

			for (int i = 0; i < get_tab_count(); i++) {
				ScriptEditorDebugger *n = Object::cast_to<ScriptEditorDebugger>(get_tab_control(i));
				if (!n)
					continue;
				n->update_tabs();
			}

			if (error_count == 0 && warning_count == 0) {
				debugger_button->set_text(TTR("Debugger"));
				debugger_button->set_icon(Ref<Texture>());
			} else {
				debugger_button->set_text(TTR("Debugger") + " (" + itos(error_count + warning_count) + ")");
				if (error_count == 0) {
					debugger_button->set_icon(get_icon("Warning", "EditorIcons"));
				} else {
					debugger_button->set_icon(get_icon("Error", "EditorIcons"));
				}
			}
			last_error_count = error_count;
			last_warning_count = warning_count;
		}

		// Take connections
		if (get_tab_count() >= 4) { // XXX Allow more?
			// We already have a valid connection. Disconnecting any new connecting client to prevent it from hanging.
			// (If we don't keep a reference to the connection it will be destroyed and disconnect_from_host will be called internally)
			server->take_connection();
			return;
		} else {
			ScriptEditorDebugger *extra;
			if (debugger->is_processing()) {
				extra = _add_debugger("Session " + itos(get_tab_count()));
			} else {
				extra = debugger;
				EditorNode::get_log()->add_message("--- Debugging process started ---", EditorLog::MSG_TYPE_EDITOR);
				EditorNode::get_singleton()->get_pause_button()->set_pressed(false);
				EditorNode::get_singleton()->get_pause_button()->set_disabled(false);
				debugger->update_live_edit_root();
			}
			extra->start(server->take_connection());
		}
	}
}

void EditorDebuggerNode::_breaked(bool p_breaked, bool p_can_debug) {
	EditorNode::get_singleton()->get_pause_button()->set_pressed(true);
	EditorNode::get_singleton()->make_bottom_panel_item_visible(this);
	emit_signal("breaked", p_breaked, p_can_debug);
}
