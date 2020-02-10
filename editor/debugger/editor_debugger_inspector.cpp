#include "editor_debugger_inspector.h"

#include "editor/editor_node.h"
#include "scene/debugger/scene_debugger.h"

class EditorDebuggerRemoteObject : public Object {

	GDCLASS(EditorDebuggerRemoteObject, Object);

protected:
	bool _set(const StringName &p_name, const Variant &p_value) {

		if (!editable || !prop_values.has(p_name) || String(p_name).begins_with("Constants/"))
			return false;

		prop_values[p_name] = p_value;
		emit_signal("value_edited", p_name, p_value);
		return true;
	}

	bool _get(const StringName &p_name, Variant &r_ret) const {

		if (!prop_values.has(p_name))
			return false;

		r_ret = prop_values[p_name];
		return true;
	}

	void _get_property_list(List<PropertyInfo> *p_list) const {

		p_list->clear(); //sorry, no want category
		for (const List<PropertyInfo>::Element *E = prop_list.front(); E; E = E->next()) {
			p_list->push_back(E->get());
		}
	}

	static void _bind_methods() {

		ClassDB::bind_method(D_METHOD("get_title"), &EditorDebuggerRemoteObject::get_title);
		ClassDB::bind_method(D_METHOD("get_variant"), &EditorDebuggerRemoteObject::get_variant);
		ClassDB::bind_method(D_METHOD("clear"), &EditorDebuggerRemoteObject::clear);
		ClassDB::bind_method(D_METHOD("get_remote_object_id"), &EditorDebuggerRemoteObject::get_remote_object_id);

		ADD_SIGNAL(MethodInfo("value_edited"));
	}

public:
	bool editable;
	String type_name;
	ObjectID remote_object_id;
	List<PropertyInfo> prop_list;
	Map<StringName, Variant> prop_values;

	ObjectID get_remote_object_id() {
		return remote_object_id;
	}

	String get_title() {
		if (remote_object_id)
			return TTR("Remote ") + String(type_name) + ": " + itos(remote_object_id);
		else
			return "<null>";
	}
	Variant get_variant(const StringName &p_name) {

		Variant var;
		_get(p_name, var);
		return var;
	}

	void clear() {
		prop_list.clear();
		prop_values.clear();
	}

	void update() {
		_change_notify();
	}

	void update_single(const char *p_prop) {
		_change_notify(p_prop);
	}

	EditorDebuggerRemoteObject() {
		remote_object_id = 0;
		editable = true;
	}
};

EditorDebuggerInspector::EditorDebuggerInspector() {
	inspected_object_id = 0;
	variables = memnew(EditorDebuggerRemoteObject);
	variables->editable = false;
}

EditorDebuggerInspector::~EditorDebuggerInspector() {
	memdelete(variables);
}

void EditorDebuggerInspector::_bind_methods() {
}

void EditorDebuggerInspector::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE:
			connect("object_id_selected", this, "_object_selected");
			break;
		case NOTIFICATION_ENTER_TREE:
			edit(variables);
			break;
		default:
			break;
	}
}

void EditorDebuggerInspector::_object_edited(const String &p_prop, const Variant &p_value) {

	emit_signal("update_object", inspected_object_id, p_prop, p_value);
}

void EditorDebuggerInspector::_object_selected(ObjectID p_object) {

	inspected_object_id = p_object;
	emit_signal("inspect_object", p_object);
}

void EditorDebuggerInspector::add_object(const Array &p_arr) {
	EditorDebuggerRemoteObject *debugObj = NULL;

	SceneDebuggerObject obj;
	obj.deserialize(p_arr);
	ERR_FAIL_COND(obj.id == 0);

	if (remote_objects.has(obj.id)) {
		debugObj = remote_objects[obj.id];
	} else {
		debugObj = memnew(EditorDebuggerRemoteObject);
		debugObj->remote_object_id = obj.id;
		debugObj->type_name = obj.class_name;
		remote_objects[obj.id] = debugObj;
		debugObj->connect("value_edited", this, "_scene_tree_property_value_edited");
	}

	int old_prop_size = debugObj->prop_list.size();

	debugObj->prop_list.clear();
	int new_props_added = 0;
	Set<String> changed;
	for (int i = 0; i < obj.properties.size(); i++) {

		PropertyInfo &pinfo = obj.properties[i].first;
		Variant &var = obj.properties[i].second;

		if (pinfo.type == Variant::OBJECT) {
			if (var.get_type() == Variant::STRING) {
				String path = var;
				if (path.find("::") != -1) {
					// built-in resource
					String base_path = path.get_slice("::", 0);
					if (ResourceLoader::get_resource_type(base_path) == "PackedScene") {
						if (!EditorNode::get_singleton()->is_scene_open(base_path)) {
							EditorNode::get_singleton()->load_scene(base_path);
						}
					} else {
						EditorNode::get_singleton()->load_resource(base_path);
					}
				}
				var = ResourceLoader::load(path);

				if (pinfo.hint_string == "Script") {
					if (debugObj->get_script() != var) {
						debugObj->set_script(RefPtr());
						Ref<Script> script(var);
						if (!script.is_null()) {
							ScriptInstance *script_instance = script->placeholder_instance_create(debugObj);
							debugObj->set_script_and_instance(var, script_instance);
						}
					}
				}
			}
		}

		//always add the property, since props may have been added or removed
		debugObj->prop_list.push_back(pinfo);

		if (!debugObj->prop_values.has(pinfo.name)) {
			new_props_added++;
			debugObj->prop_values[pinfo.name] = var;
		} else {

			if (bool(Variant::evaluate(Variant::OP_NOT_EQUAL, debugObj->prop_values[pinfo.name], var))) {
				debugObj->prop_values[pinfo.name] = var;
				changed.insert(pinfo.name);
			}
		}
	}

	if (old_prop_size == debugObj->prop_list.size() && new_props_added == 0) {
		//only some may have changed, if so, then update those, if exist
		for (Set<String>::Element *E = changed.front(); E; E = E->next()) {
			// TODO update properties...
			//EditorNode::get_singleton()->get_inspector()->update_property(E->get());
		}
	} else {
		//full update, because props were added or removed
		debugObj->update();
	}
}

void EditorDebuggerInspector::clear_cache() {
	for (Map<ObjectID, EditorDebuggerRemoteObject *>::Element *E = remote_objects.front(); E; E = E->next()) {
		EditorNode *editor = EditorNode::get_singleton();
		if (editor->get_editor_history()->get_current() == E->value()->get_instance_id()) {
			editor->push_item(NULL);
		}
		memdelete(E->value());
	}
	remote_objects.clear();
}

void EditorDebuggerInspector::add_property(const String &p_name, const Variant &p_value, const PropertyHint &p_hint, const String p_hint_string) {

	PropertyInfo pinfo;
	pinfo.name = p_name;
	pinfo.type = p_value.get_type();
	pinfo.hint = p_hint;
	pinfo.hint_string = p_hint_string;

	variables->prop_list.push_back(pinfo);
	variables->prop_values[p_name] = p_value;
	variables->update();
	edit(variables);
}

void EditorDebuggerInspector::clear_properties() {
	variables->clear();
}

String EditorDebuggerInspector::get_var_value(const String &p_var) {
	return variables->get_variant(p_var);
}
