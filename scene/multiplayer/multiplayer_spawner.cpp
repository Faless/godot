#include "multiplayer_spawner.h"

#include "core/io/marshalls.h"
#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/window.h"

void MultiplayerSpawner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("spawn", "node", "peer"), &MultiplayerSpawner::spawn, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("get_spawnable_scenes"), &MultiplayerSpawner::get_spawnable_scenes);
	ClassDB::bind_method(D_METHOD("set_spawnable_scenes", "scenes"), &MultiplayerSpawner::set_spawnable_scenes);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "replication", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), (PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE)), "set_spawnable_scenes", "get_spawnable_scenes");

	ClassDB::bind_method(D_METHOD("get_spawn_path"), &MultiplayerSpawner::get_spawn_path);
	ClassDB::bind_method(D_METHOD("set_spawn_path", "path"), &MultiplayerSpawner::set_spawn_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "spawn_path", PROPERTY_HINT_NONE, ""), "set_spawn_path", "get_spawn_path");

	ADD_SIGNAL(MethodInfo("despawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("spawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
}

void MultiplayerSpawner::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		//get_multiplayer()->get_replicator()->register_spawner(get_path(), &_spawn_despawn_cb);
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		//get_multiplayer()->get_replicator()->deregister_spawner(get_path());
	}
}

TypedArray<PackedScene> MultiplayerSpawner::get_spawnable_scenes() {
	return spawnable_scenes;
}

void MultiplayerSpawner::set_spawnable_scenes(TypedArray<PackedScene> p_scenes) {
	spawnable_scenes = p_scenes;
	spawnable_ids.clear();
	for (int i = 0; i < p_scenes.size(); i++) {
		Ref<PackedScene> scene = p_scenes[i];
		if (scene.is_null() || scene->get_path().is_empty()) {
			continue;
		}
		ResourceUID::ID id = ResourceLoader::get_resource_uid(scene->get_path());
		ERR_CONTINUE(id == ResourceUID::INVALID_ID);
		spawnable_ids.insert(id);
	}
}

NodePath MultiplayerSpawner::get_spawn_path() const {
	return spawn_path;
}

void MultiplayerSpawner::set_spawn_path(const NodePath &p_path) {
	spawn_path = p_path;
}

Node *MultiplayerSpawner::get_currently_spawning() {
	return spawning;
}

Error MultiplayerSpawner::spawn(Node *p_node, int p_peer) {
	ERR_FAIL_COND_V(!is_inside_tree(), ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(spawn_path.is_empty() || !has_node(spawn_path), ERR_UNCONFIGURED);
	const String scene = p_node->get_scene_file_path();
	if (scene.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	ResourceUID::ID id = ResourceLoader::get_resource_uid(scene);
	if (!spawnable_ids.has(id)) {
		return ERR_INVALID_PARAMETER;
	}
	Node *parent = get_node(spawn_path);
	track(p_node);
	parent->add_child(p_node);
	return get_multiplayer()->spawn(p_node, p_peer);
}

void MultiplayerSpawner::track(Node *p_node) {
	ObjectID oid = p_node->get_instance_id();
	if (!tracked_nodes.has(oid)) {
		tracked_nodes.insert(oid);
		_connect_node(p_node);
		get_multiplayer()->replication_start(p_node, this);
	}
}

#if 0
Error MultiplayerSpawner::remote_spawn(Node *p_node, const String &p_name) {
	ERR_FAIL_COND_V(spawn_path.is_empty() || !has_node(spawn_path), ERR_UNCONFIGURED);
	Node *parent = get_node(spawn_path);
	ERR_FAIL_COND_V(parent->has_node(p_name), ERR_INVALID_DATA);
	get_node(spawn_path)->_add_child_nocheck(node, p_name);
	remote_nodes.insert(node->get_instance_id());
	return OK;
}
#endif

Error MultiplayerSpawner::remote_spawn(int p_from, const ResourceUID::ID &p_scene_id, const String &p_name, const PackedByteArray &p_state, ObjectID &r_id) {
	ERR_FAIL_COND_V(!spawnable_ids.has(p_scene_id), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(p_from != get_multiplayer_authority(), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(!has_node(spawn_path), ERR_UNCONFIGURED);
	String scene_path = ResourceUID::get_singleton()->get_id_path(p_scene_id);
	RES res = ResourceLoader::load(scene_path);
	ERR_FAIL_COND_V_MSG(!res.is_valid(), ERR_CANT_OPEN, "Unable to load scene to spawn at path: " + scene_path);
	PackedScene *scene = Object::cast_to<PackedScene>(res.ptr());
	ERR_FAIL_COND_V(!scene, ERR_CANT_OPEN);
	Node *parent = get_node(spawn_path);
	ERR_FAIL_COND_V(parent->has_node(p_name), ERR_INVALID_DATA);
	Node *node = scene->instantiate();
	ERR_FAIL_COND_V(!node, ERR_CANT_CREATE);
	remote_nodes.insert(node->get_instance_id());
	// TODO apply state.
	_connect_node(node);
	get_node(spawn_path)->_add_child_nocheck(node, p_name);
	r_id = node->get_instance_id();
	return OK;
}

Error MultiplayerSpawner::remote_despawn(int p_from, Node *p_node) {
	ERR_FAIL_COND_V(p_from != get_multiplayer_authority(), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(!remote_nodes.has(p_node->get_instance_id()), ERR_UNAUTHORIZED);
	NodePath path = NodePath(String(spawn_path));
	ERR_FAIL_COND_V(!has_node(path), ERR_UNCONFIGURED);
	Node *parent = get_node(path);
	ERR_FAIL_COND_V(!p_node || p_node->get_parent() != parent, ERR_INVALID_DATA);
	p_node->queue_delete();
	return OK;
}

void MultiplayerSpawner::_connect_node(Node *p_node) {
	p_node->connect(SNAME("tree_exiting"), callable_mp(this, &MultiplayerSpawner::_node_exit), varray(p_node->get_instance_id()), CONNECT_ONESHOT);
}

void MultiplayerSpawner::_node_exit(ObjectID p_id) {
	if (remote_nodes.has(p_id)) {
		remote_nodes.erase(p_id);
	}
	if (tracked_nodes.has(p_id)) {
		Node *node = Object::cast_to<Node>(ObjectDB::get_instance(p_id));
		ERR_FAIL_COND(!node);
		get_multiplayer()->despawn(node);
		tracked_nodes.erase(p_id);
		get_multiplayer()->replication_stop(node, this);
	}
}
