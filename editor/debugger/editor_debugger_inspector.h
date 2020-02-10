
#ifndef EDITOR_DEBUGGER_REMOTE_INSPECTOR_H
#define EDITOR_DEBUGGER_REMOTE_INSPECTOR_H
#include "editor/editor_inspector.h"

class EditorDebuggerRemoteObject;

class EditorDebuggerInspector : public EditorInspector {

	GDCLASS(EditorDebuggerInspector, EditorInspector);

private:
	ObjectID inspected_object_id;
	Map<ObjectID, EditorDebuggerRemoteObject *> remote_objects;
	EditorDebuggerRemoteObject *variables;

	void _object_selected(ObjectID p_object);
	void _object_edited(const String &p_prop, const Variant &p_value);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	EditorDebuggerInspector();
	~EditorDebuggerInspector();

	// Remote Object cache
	ObjectID add_object(const Array &p_arr);
	EditorDebuggerRemoteObject *get_object(ObjectID p_id);
	void clear_cache();

	// Stack Dump variables
	String get_stack_variable(const String &p_var);
	void add_stack_variable(const Array &p_arr);
	void clear_stack_variables();
};

#endif // EDITOR_DEBUGGER_REMOTE_INSPECTOR_H
