#include "multiplayer_spawner.h"

#include "core/io/marshalls.h"
#include "core/multiplayer/multiplayer_replicator.h"
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
	spawning = p_node;
	Error err = get_multiplayer()->spawn(this, p_peer);
	spawning = nullptr;
	return err;
}

Error MultiplayerSpawner::local_spawn() {
	ERR_FAIL_COND_V(!spawning, ERR_BUG);
	ERR_FAIL_COND_V(spawn_path.is_empty() || !has_node(spawn_path), ERR_BUG);
	Node *parent = get_node(spawn_path);
	parent->add_child(spawning);
	return OK;
}

Error MultiplayerSpawner::remote_spawn(int p_from, const ResourceUID::ID &p_scene_id, const String &p_name, const PackedByteArray &p_state) {
	ERR_FAIL_COND_V(!spawnable_ids.has(p_scene_id), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(p_from != get_multiplayer_authority(), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(!has_node(spawn_path), ERR_UNCONFIGURED);
	String scene_path = ResourceUID::get_singleton()->get_id_path(p_scene_id);
	RES res = ResourceLoader::load(scene_path);
	ERR_FAIL_COND_V_MSG(!res.is_valid(), ERR_CANT_OPEN, "Unable to load scene to spawn at path: " + scene_path);
	PackedScene *scene = Object::cast_to<PackedScene>(res.ptr());
	ERR_FAIL_COND_V(!scene, ERR_CANT_OPEN);
	Node *node = scene->instantiate();
	ERR_FAIL_COND_V(!node, ERR_CANT_CREATE);
	// TODO apply state.
	get_node(spawn_path)->_add_child_nocheck(node, p_name);
	return OK;
}

Error MultiplayerSpawner::remote_despawn(int p_from, const ResourceUID::ID &p_scene_id, const String &p_name, const PackedByteArray &p_state) {
	ERR_FAIL_COND_V(!spawnable_ids.has(p_scene_id), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(p_from != get_multiplayer_authority(), ERR_UNAUTHORIZED);
	NodePath path = NodePath(String(spawn_path) + "/" + p_name);
	ERR_FAIL_COND_V(!has_node(path), ERR_UNCONFIGURED);
	// TODO confirm this was remotely spawned!
	get_node(path)->queue_delete();
	return OK;
}
