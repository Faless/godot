/*************************************************************************/
/*  multiplayer_spawner.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "multiplayer_spawner.h"

#include "core/io/marshalls.h"
#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/window.h"

void MultiplayerSpawner::_bind_methods() {
	ClassDB::bind_method(D_METHOD("spawn", "data"), &MultiplayerSpawner::spawn);

	ClassDB::bind_method(D_METHOD("get_spawnable_scenes"), &MultiplayerSpawner::get_spawnable_scenes);
	ClassDB::bind_method(D_METHOD("set_spawnable_scenes", "scenes"), &MultiplayerSpawner::set_spawnable_scenes);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "replication", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), (PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_SCRIPT_VARIABLE)), "set_spawnable_scenes", "get_spawnable_scenes");

	ClassDB::bind_method(D_METHOD("get_spawn_path"), &MultiplayerSpawner::get_spawn_path);
	ClassDB::bind_method(D_METHOD("set_spawn_path", "path"), &MultiplayerSpawner::set_spawn_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "spawn_path", PROPERTY_HINT_NONE, ""), "set_spawn_path", "get_spawn_path");

	ClassDB::bind_method(D_METHOD("set_auto_spawning", "enabled"), &MultiplayerSpawner::set_auto_spawning);
	ClassDB::bind_method(D_METHOD("is_auto_spawning"), &MultiplayerSpawner::is_auto_spawning);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_spawn"), "set_auto_spawning", "is_auto_spawning");

	GDVIRTUAL_BIND(_spawn_custom, "data");
	GDVIRTUAL_BIND(_can_spawn_scene, "scene");

	ADD_SIGNAL(MethodInfo("despawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("spawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
}

void MultiplayerSpawner::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (auto_spawn) {
			get_tree()->connect("node_added", callable_mp(this, &MultiplayerSpawner::_node_added));
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		if (auto_spawn) {
			get_tree()->disconnect("node_added", callable_mp(this, &MultiplayerSpawner::_node_added));
		}
	}
}

void MultiplayerSpawner::_node_added(Node *p_node) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	const Ref<MultiplayerAPI> multiplayer = get_multiplayer();
	if (!multiplayer->has_multiplayer_peer() || get_multiplayer_authority() != multiplayer->get_unique_id()) {
		return;
	}
	if (is_tracking(p_node)) {
		return;
	}
	if (spawn_path.is_empty() || !has_node(spawn_path)) {
		return;
	}
	const Node *parent = get_node(spawn_path);
	if (p_node->get_parent() != parent) {
		return;
	}
	if (!can_spawn_scene(p_node->get_scene_file_path())) {
		return;
	}
	const String name = p_node->get_name();
	ERR_FAIL_COND_MSG(name.validate_node_name() != name, vformat("Unable to auto-spawn node with reserved name: %s. Make sure to add your replicated scenes via 'add_child(node, true)' to produce valid names.", name));
	track(p_node);
}

void MultiplayerSpawner::set_auto_spawning(bool p_enabled) {
	if (p_enabled != auto_spawn && is_inside_tree()) {
		if (p_enabled) {
			get_tree()->connect("node_added", callable_mp(this, &MultiplayerSpawner::_node_added));
		} else {
			get_tree()->disconnect("node_added", callable_mp(this, &MultiplayerSpawner::_node_added));
		}
	}
	auto_spawn = p_enabled;
}

bool MultiplayerSpawner::is_auto_spawning() const {
	return auto_spawn;
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

bool MultiplayerSpawner::is_tracking(const Node *p_node) const {
	return tracked_nodes.has(p_node->get_instance_id());
}

void MultiplayerSpawner::track(Node *p_node) {
	ObjectID oid = p_node->get_instance_id();
	if (!tracked_nodes.has(oid)) {
		tracked_nodes.insert(oid);
		_connect_node(p_node);
		get_multiplayer()->replication_start(p_node, this);
	}
}

void MultiplayerSpawner::_connect_node(Node *p_node) {
	p_node->connect(SNAME("tree_exiting"), callable_mp(this, &MultiplayerSpawner::_node_exit), varray(p_node->get_instance_id()), CONNECT_ONESHOT);
}

void MultiplayerSpawner::_node_exit(ObjectID p_id) {
	Node *node = Object::cast_to<Node>(ObjectDB::get_instance(p_id));
	ERR_FAIL_COND(!node);
	if (tracked_nodes.has(p_id)) {
		tracked_nodes.erase(p_id);
		get_multiplayer()->replication_stop(node, this);
	}
}

bool MultiplayerSpawner::can_spawn_scene(const String &p_scene) {
	bool can_spawn = false;
	if (GDVIRTUAL_CALL(_can_spawn_scene, p_scene, can_spawn)) {
		return can_spawn;
	} else {
		ResourceUID::ID id = ResourceLoader::get_resource_uid(p_scene);
		return spawnable_ids.has(id);
	}
}

Node *MultiplayerSpawner::spawn(const Variant &p_data) {
	Object *obj = nullptr;
	Node *node = nullptr;
	if (GDVIRTUAL_CALL(_spawn_custom, p_data, obj)) {
		node = Object::cast_to<Node>(obj);
	}
	ERR_FAIL_COND_V_MSG(!node, nullptr, "Custom spawn requires the '_spawn_custom' virtual method to be implemented via script. The method must return a valid Node.");
	Node *parent = get_node(spawn_path);
	track(node);
	parent->add_child(node, true);
	return node;
}
