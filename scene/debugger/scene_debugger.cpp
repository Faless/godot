#include "scene_debugger.h"

#include "core/io/marshalls.h"
#include "core/script_debugger_remote.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"

void SceneDebugger::add_to_cache(const String &p_filename, Node *p_node) {
	LiveEditor *debugger = LiveEditor::get_singleton();
	if (!debugger)
		return;

	if (ScriptDebugger::get_singleton() && p_filename != String()) {
		//used for live edit
		debugger->live_scene_edit_cache[p_filename].insert(p_node);
	}
}
void SceneDebugger::remove_from_cache(const String &p_filename, Node *p_node) {
	LiveEditor *debugger = LiveEditor::get_singleton();
	if (!debugger)
		return;

	Map<String, Set<Node *> > &edit_cache = debugger->live_scene_edit_cache;
	Map<String, Set<Node *> >::Element *E = edit_cache.find(p_filename);
	if (E) {
		E->get().erase(p_node);
		if (E->get().size() == 0) {
			edit_cache.erase(E);
		}
	}

	Map<Node *, Map<ObjectID, Node *> > &remove_list = debugger->live_edit_remove_list;
	Map<Node *, Map<ObjectID, Node *> >::Element *F = remove_list.find(p_node);
	if (F) {
		for (Map<ObjectID, Node *>::Element *G = F->get().front(); G; G = G->next()) {

			memdelete(G->get());
		}
		remove_list.erase(F);
	}
}

void SceneDebugger::initialize() {
	LiveEditor::singleton = memnew(LiveEditor);
	ScriptDebuggerRemote::scene_tree_parse_func = SceneDebugger::parse_message;
}

void SceneDebugger::deinitialize() {
	if (LiveEditor::singleton) {
		memdelete(LiveEditor::singleton);
		LiveEditor::singleton = NULL;
	}
}

bool SceneDebugger::parse_message(const String &p_msg, const Array &p_args) {
#ifdef DEBUG_ENABLED
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return false;
	LiveEditor *live_editor = LiveEditor::get_singleton();
	if (!live_editor)
		return false;
	if (p_msg == "request_scene_tree") { // Scene tree
		live_editor->_send_tree();

	} else if (p_msg == "save_node") { // Save node.
		ERR_FAIL_COND_V(p_args.size() < 2, true);
		_save_node(p_args[0], p_args[1]);

	} else if (p_msg == "inspect_object") { // Object Inspect
		ERR_FAIL_COND_V(p_args.size() < 1, true);
		ObjectID id = p_args[0];
		_send_object_id(id);

	} else if (p_msg == "override_camera_2D:set") { // Camera
		ERR_FAIL_COND_V(p_args.size() < 1, true);
		bool enforce = p_args[0];
		scene_tree->get_root()->enable_canvas_transform_override(enforce);

	} else if (p_msg == "override_camera_2D:transform") {
		ERR_FAIL_COND_V(p_args.size() < 1, true);
		Transform2D transform = p_args[1];
		scene_tree->get_root()->set_canvas_transform_override(transform);

	} else if (p_msg == "override_camera_3D:set") {
		ERR_FAIL_COND_V(p_args.size() < 1, true);
		bool enable = p_args[0];
		scene_tree->get_root()->enable_camera_override(enable);

	} else if (p_msg == "override_camera_3D:transform") {
		ERR_FAIL_COND_V(p_args.size() < 5, true);
		Transform transform = p_args[0];
		bool is_perspective = p_args[1];
		float size_or_fov = p_args[2];
		float near = p_args[3];
		float far = p_args[4];
		if (is_perspective) {
			scene_tree->get_root()->set_camera_override_perspective(size_or_fov, near, far);
		} else {
			scene_tree->get_root()->set_camera_override_orthogonal(size_or_fov, near, far);
		}
		scene_tree->get_root()->set_camera_override_transform(transform);

	} else if (p_msg == "set_object_property") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		_set_object_property(p_args[0], p_args[1], p_args[2]);

	} else if (!p_msg.begins_with("live_")) { // Live edits below.
		return false;
	} else if (p_msg == "live_set_root") {
		ERR_FAIL_COND_V(p_args.size() < 2, true);
		live_editor->_root_func(p_args[0], p_args[1]);

	} else if (p_msg == "live_node_path") {
		ERR_FAIL_COND_V(p_args.size() < 2, true);
		live_editor->_node_path_func(p_args[0], p_args[1]);

	} else if (p_msg == "live_res_path") {
		ERR_FAIL_COND_V(p_args.size() < 2, true);
		live_editor->_res_path_func(p_args[0], p_args[1]);

	} else if (p_msg == "live_node_prop_res") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		live_editor->_node_set_res_func(p_args[0], p_args[1], p_args[2]);

	} else if (p_msg == "live_node_prop") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		live_editor->_node_set_func(p_args[0], p_args[1], p_args[2]);

	} else if (p_msg == "live_res_prop_res") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		live_editor->_res_set_res_func(p_args[0], p_args[1], p_args[2]);

	} else if (p_msg == "live_res_prop") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		live_editor->_res_set_func(p_args[0], p_args[1], p_args[2]);

	} else if (p_msg == "live_node_call") {
		ERR_FAIL_COND_V(p_args.size() < 7, true);
		live_editor->_node_call_func(p_args[0], p_args[1], p_args[2], p_args[3], p_args[4], p_args[5], p_args[6]);

	} else if (p_msg == "live_res_call") {
		ERR_FAIL_COND_V(p_args.size() < 7, true);
		live_editor->_res_call_func(p_args[0], p_args[1], p_args[2], p_args[3], p_args[4], p_args[5], p_args[6]);

	} else if (p_msg == "live_create_node") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		live_editor->_create_node_func(p_args[0], p_args[1], p_args[2]);

	} else if (p_msg == "live_instance_node") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		live_editor->_instance_node_func(p_args[0], p_args[1], p_args[2]);

	} else if (p_msg == "live_remove_node") {
		ERR_FAIL_COND_V(p_args.size() < 1, true);
		live_editor->_remove_node_func(p_args[0]);

	} else if (p_msg == "live_remove_and_keep_node") {
		ERR_FAIL_COND_V(p_args.size() < 2, true);
		live_editor->_remove_and_keep_node_func(p_args[0], p_args[1]);

	} else if (p_msg == "live_restore_node") {
		ERR_FAIL_COND_V(p_args.size() < 3, true);
		live_editor->_restore_node_func(p_args[0], p_args[1], p_args[2]);

	} else if (p_msg == "live_duplicate_node") {
		ERR_FAIL_COND_V(p_args.size() < 2, true);
		live_editor->_duplicate_node_func(p_args[0], p_args[1]);

	} else if (p_msg == "live_reparent_node") {
		ERR_FAIL_COND_V(p_args.size() < 4, true);
		live_editor->_reparent_node_func(p_args[0], p_args[1], p_args[2], p_args[3]);
	} else {
		return false;
	}
	return true;
#else
	return false;
#endif
}

void SceneDebugger::_save_node(ObjectID id, const String &p_path) {
#ifdef DEBUG_ENABLED
	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(id));
	ERR_FAIL_COND(!node);

	Ref<PackedScene> ps = memnew(PackedScene);
	ps->pack(node);
	ResourceSaver::save(p_path, ps);
#endif
}

void SceneDebugger::_send_object_id(ObjectID p_id, int p_max_size) {
#ifdef DEBUG_ENABLED
	Object *obj = ObjectDB::get_instance(p_id);
	if (!obj)
		return;

	typedef Pair<PropertyInfo, Variant> PropertyDesc;
	List<PropertyDesc> properties;

	if (ScriptInstance *si = obj->get_script_instance()) {
		if (!si->get_script().is_null()) {

			typedef Map<const Script *, Set<StringName> > ScriptMemberMap;
			typedef Map<const Script *, Map<StringName, Variant> > ScriptConstantsMap;

			ScriptMemberMap members;
			members[si->get_script().ptr()] = Set<StringName>();
			si->get_script()->get_members(&(members[si->get_script().ptr()]));

			ScriptConstantsMap constants;
			constants[si->get_script().ptr()] = Map<StringName, Variant>();
			si->get_script()->get_constants(&(constants[si->get_script().ptr()]));

			Ref<Script> base = si->get_script()->get_base_script();
			while (base.is_valid()) {

				members[base.ptr()] = Set<StringName>();
				base->get_members(&(members[base.ptr()]));

				constants[base.ptr()] = Map<StringName, Variant>();
				base->get_constants(&(constants[base.ptr()]));

				base = base->get_base_script();
			}

			for (ScriptMemberMap::Element *sm = members.front(); sm; sm = sm->next()) {
				for (Set<StringName>::Element *E = sm->get().front(); E; E = E->next()) {
					Variant m;
					if (si->get(E->get(), m)) {
						String script_path = sm->key() == si->get_script().ptr() ? "" : sm->key()->get_path().get_file() + "/";
						PropertyInfo pi(m.get_type(), "Members/" + script_path + E->get());
						properties.push_back(PropertyDesc(pi, m));
					}
				}
			}

			for (ScriptConstantsMap::Element *sc = constants.front(); sc; sc = sc->next()) {
				for (Map<StringName, Variant>::Element *E = sc->get().front(); E; E = E->next()) {
					String script_path = sc->key() == si->get_script().ptr() ? "" : sc->key()->get_path().get_file() + "/";
					if (E->value().get_type() == Variant::OBJECT) {
						Variant id = ((Object *)E->value())->get_instance_id();
						PropertyInfo pi(id.get_type(), "Constants/" + E->key(), PROPERTY_HINT_OBJECT_ID, "Object");
						properties.push_back(PropertyDesc(pi, id));
					} else {
						PropertyInfo pi(E->value().get_type(), "Constants/" + script_path + E->key());
						properties.push_back(PropertyDesc(pi, E->value()));
					}
				}
			}
		}
	}

	if (Node *node = Object::cast_to<Node>(obj)) {
		// in some cases node will not be in tree here
		// for instance where it created as variable and not yet added to tree
		// in such cases we can't ask for it's path
		if (node->is_inside_tree()) {
			PropertyInfo pi(Variant::NODE_PATH, String("Node/path"));
			properties.push_front(PropertyDesc(pi, node->get_path()));
		} else {
			PropertyInfo pi(Variant::STRING, String("Node/path"));
			properties.push_front(PropertyDesc(pi, "[Orphan]"));
		}

	} else if (Resource *res = Object::cast_to<Resource>(obj)) {
		if (Script *s = Object::cast_to<Script>(res)) {
			Map<StringName, Variant> constants;
			s->get_constants(&constants);
			for (Map<StringName, Variant>::Element *E = constants.front(); E; E = E->next()) {
				if (E->value().get_type() == Variant::OBJECT) {
					Variant id = ((Object *)E->value())->get_instance_id();
					PropertyInfo pi(id.get_type(), "Constants/" + E->key(), PROPERTY_HINT_OBJECT_ID, "Object");
					properties.push_front(PropertyDesc(pi, E->value()));
				} else {
					PropertyInfo pi(E->value().get_type(), String("Constants/") + E->key());
					properties.push_front(PropertyDesc(pi, E->value()));
				}
			}
		}
	}

	List<PropertyInfo> pinfo;
	obj->get_property_list(&pinfo, true);
	for (List<PropertyInfo>::Element *E = pinfo.front(); E; E = E->next()) {
		if (E->get().usage & (PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_CATEGORY)) {
			properties.push_back(PropertyDesc(E->get(), obj->get(E->get().name)));
		}
	}

	Array send_props;
	for (int i = 0; i < properties.size(); i++) {
		const PropertyInfo &pi = properties[i].first;
		Variant &var = properties[i].second;

		WeakRef *ref = Object::cast_to<WeakRef>(var);
		if (ref) {
			var = ref->get_ref();
		}

		RES res = var;

		Array prop;
		prop.push_back(pi.name);
		prop.push_back(pi.type);

		//only send information that can be sent..
		int len = 0; //test how big is this to encode
		encode_variant(var, NULL, len);
		if (len > p_max_size) { //limit to max size
			prop.push_back(PROPERTY_HINT_OBJECT_TOO_BIG);
			prop.push_back("");
			prop.push_back(pi.usage);
			prop.push_back(Variant());
		} else {
			prop.push_back(pi.hint);
			prop.push_back(pi.hint_string);
			prop.push_back(pi.usage);

			if (!res.is_null()) {
				var = res->get_path();
			}

			prop.push_back(var);
		}
		send_props.push_back(prop);
	}

	Array arr;
	arr.push_back(p_id);
	arr.push_back(obj->get_class());
	arr.push_back(send_props);
	ScriptDebugger::get_singleton()->send_message("inspect_object", arr);
#endif
}

void SceneDebugger::_set_object_property(ObjectID p_id, const String &p_property, const Variant &p_value) {
#ifdef DEBUG_ENABLED

	Object *obj = ObjectDB::get_instance(p_id);
	if (!obj)
		return;

	String prop_name = p_property;
	if (p_property.begins_with("Members/")) {
		Vector<String> ss = p_property.split("/");
		prop_name = ss[ss.size() - 1];
	}

	obj->set(prop_name, p_value);
#endif
}

/// LiveEditor
LiveEditor *LiveEditor::singleton = NULL;
LiveEditor *LiveEditor::get_singleton() {
	return singleton;
}

#ifdef DEBUG_ENABLED

static void _fill_array(Node *p_node, Array &array, int p_level) {

	array.push_back(p_node->get_child_count());
	array.push_back(p_node->get_name());
	array.push_back(p_node->get_class());
	array.push_back(p_node->get_instance_id());
	for (int i = 0; i < p_node->get_child_count(); i++) {

		_fill_array(p_node->get_child(i), array, p_level + 1);
	}
}

void LiveEditor::_send_tree() {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Array arr;
	_fill_array(scene_tree->root, arr, 0);
	ScriptDebugger::get_singleton()->send_message("scene_tree", arr);
}

void LiveEditor::_node_path_func(const NodePath &p_path, int p_id) {

	live_edit_node_path_cache[p_id] = p_path;
}

void LiveEditor::_res_path_func(const String &p_path, int p_id) {

	live_edit_resource_cache[p_id] = p_path;
}

void LiveEditor::_node_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	if (!live_edit_node_path_cache.has(p_id))
		return;

	NodePath np = live_edit_node_path_cache[p_id];
	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(np))
			continue;
		Node *n2 = n->get_node(np);

		n2->set(p_prop, p_value);
	}
}

void LiveEditor::_node_set_res_func(int p_id, const StringName &p_prop, const String &p_value) {

	RES r = ResourceLoader::load(p_value);
	if (!r.is_valid())
		return;
	_node_set_func(p_id, p_prop, r);
}
void LiveEditor::_node_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;
	if (!live_edit_node_path_cache.has(p_id))
		return;

	NodePath np = live_edit_node_path_cache[p_id];
	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(np))
			continue;
		Node *n2 = n->get_node(np);

		n2->call(p_method, VARIANT_ARG_PASS);
	}
}
void LiveEditor::_res_set_func(int p_id, const StringName &p_prop, const Variant &p_value) {

	if (!live_edit_resource_cache.has(p_id))
		return;

	String resp = live_edit_resource_cache[p_id];

	if (!ResourceCache::has(resp))
		return;

	RES r = ResourceCache::get(resp);
	if (!r.is_valid())
		return;

	r->set(p_prop, p_value);
}
void LiveEditor::_res_set_res_func(int p_id, const StringName &p_prop, const String &p_value) {

	RES r = ResourceLoader::load(p_value);
	if (!r.is_valid())
		return;
	_res_set_func(p_id, p_prop, r);
}
void LiveEditor::_res_call_func(int p_id, const StringName &p_method, VARIANT_ARG_DECLARE) {

	if (!live_edit_resource_cache.has(p_id))
		return;

	String resp = live_edit_resource_cache[p_id];

	if (!ResourceCache::has(resp))
		return;

	RES r = ResourceCache::get(resp);
	if (!r.is_valid())
		return;

	r->call(p_method, VARIANT_ARG_PASS);
}

void LiveEditor::_root_func(const NodePath &p_scene_path, const String &p_scene_from) {

	live_edit_root = p_scene_path;
	live_edit_scene = p_scene_from;
}

void LiveEditor::_create_node_func(const NodePath &p_parent, const String &p_type, const String &p_name) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(p_parent))
			continue;
		Node *n2 = n->get_node(p_parent);

		Node *no = Object::cast_to<Node>(ClassDB::instance(p_type));
		if (!no) {
			continue;
		}

		no->set_name(p_name);
		n2->add_child(no);
	}
}
void LiveEditor::_instance_node_func(const NodePath &p_parent, const String &p_path, const String &p_name) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Ref<PackedScene> ps = ResourceLoader::load(p_path);

	if (!ps.is_valid())
		return;

	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(p_parent))
			continue;
		Node *n2 = n->get_node(p_parent);

		Node *no = ps->instance();
		if (!no) {
			continue;
		}

		no->set_name(p_name);
		n2->add_child(no);
	}
}
void LiveEditor::_remove_node_func(const NodePath &p_at) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F;) {

		Set<Node *>::Element *N = F->next();

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(p_at))
			continue;
		Node *n2 = n->get_node(p_at);

		memdelete(n2);

		F = N;
	}
}
void LiveEditor::_remove_and_keep_node_func(const NodePath &p_at, ObjectID p_keep_id) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F;) {

		Set<Node *>::Element *N = F->next();

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(p_at))
			continue;

		Node *n2 = n->get_node(p_at);

		n2->get_parent()->remove_child(n2);

		live_edit_remove_list[n][p_keep_id] = n2;

		F = N;
	}
}
void LiveEditor::_restore_node_func(ObjectID p_id, const NodePath &p_at, int p_at_pos) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F;) {

		Set<Node *>::Element *N = F->next();

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(p_at))
			continue;
		Node *n2 = n->get_node(p_at);

		Map<Node *, Map<ObjectID, Node *> >::Element *EN = live_edit_remove_list.find(n);

		if (!EN)
			continue;

		Map<ObjectID, Node *>::Element *FN = EN->get().find(p_id);

		if (!FN)
			continue;
		n2->add_child(FN->get());

		EN->get().erase(FN);

		if (EN->get().size() == 0) {
			live_edit_remove_list.erase(EN);
		}

		F = N;
	}
}
void LiveEditor::_duplicate_node_func(const NodePath &p_at, const String &p_new_name) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(p_at))
			continue;
		Node *n2 = n->get_node(p_at);

		Node *dup = n2->duplicate(Node::DUPLICATE_SIGNALS | Node::DUPLICATE_GROUPS | Node::DUPLICATE_SCRIPTS);

		if (!dup)
			continue;

		dup->set_name(p_new_name);
		n2->get_parent()->add_child(dup);
	}
}
void LiveEditor::_reparent_node_func(const NodePath &p_at, const NodePath &p_new_place, const String &p_new_name, int p_at_pos) {
	SceneTree *scene_tree = SceneTree::get_singleton();
	if (!scene_tree)
		return;

	Node *base = NULL;
	if (scene_tree->root->has_node(live_edit_root))
		base = scene_tree->root->get_node(live_edit_root);

	Map<String, Set<Node *> >::Element *E = live_scene_edit_cache.find(live_edit_scene);
	if (!E)
		return; //scene not editable

	for (Set<Node *>::Element *F = E->get().front(); F; F = F->next()) {

		Node *n = F->get();

		if (base && !base->is_a_parent_of(n))
			continue;

		if (!n->has_node(p_at))
			continue;
		Node *nfrom = n->get_node(p_at);

		if (!n->has_node(p_new_place))
			continue;
		Node *nto = n->get_node(p_new_place);

		nfrom->get_parent()->remove_child(nfrom);
		nfrom->set_name(p_new_name);

		nto->add_child(nfrom);
		if (p_at_pos >= 0)
			nto->move_child(nfrom, p_at_pos);
	}
}

#endif
