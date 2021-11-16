#include "scene_tree_replicator.h"

#include "core/io/marshalls.h"
#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/node.h"
#include "scene/multiplayer/multiplayer_spawner.h"
#include "scene/multiplayer/multiplayer_synchronizer.h"

MultiplayerReplicationInterface *SceneTreeReplicatorInterface::_create() {
	return memnew(SceneTreeReplicatorInterface);
}

void SceneTreeReplicatorInterface::make_default() {
	MultiplayerAPI::create_default_replication_interface = _create;
}

Error SceneTreeReplicatorInterface::on_replication_start(Object *p_obj, Variant p_config) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	ObjectID oid = node->get_instance_id();

	if (p_config.get_type() == Variant::ARRAY) {
		ERR_FAIL_COND_V(!tracked_objects.has(oid), ERR_INVALID_PARAMETER);
		tracked_objects[oid].args = p_config;
		return OK;
	}
	ERR_FAIL_COND_V(p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	Node *config = Object::cast_to<Node>(p_config.get_validated_object());
	ERR_FAIL_COND_V(!config, ERR_INVALID_PARAMETER);
	if (config->is_class_ptr(MultiplayerSpawner::get_class_ptr_static())) {
		return track(oid, config);
	} else if (config->is_class_ptr(MultiplayerSynchronizer::get_class_ptr_static())) {
		if (is_spawning(p_obj)) {
			_apply_spawn_state(p_obj, Object::cast_to<MultiplayerSynchronizer>(config));
		}
		return track(oid, config);
	}
	return ERR_INVALID_PARAMETER;
}

Error SceneTreeReplicatorInterface::on_replication_stop(Object *p_obj, Variant p_config) {
	ERR_FAIL_COND_V(p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	Node *config = Object::cast_to<Node>(p_config.get_validated_object());
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!config, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	ObjectID oid = node->get_instance_id();
	ERR_FAIL_COND_V(!tracked_objects.has(oid), ERR_INVALID_PARAMETER);

	if (config->is_class_ptr(MultiplayerSpawner::get_class_ptr_static())) {
		return untrack(oid, config);
	} else if (config->is_class_ptr(MultiplayerSynchronizer::get_class_ptr_static())) {
		return untrack(oid, config);
	}
	return ERR_INVALID_PARAMETER;
}

Error SceneTreeReplicatorInterface::track(const ObjectID &p_id, Object *p_config) {
	if (!tracked_objects.has(p_id)) {
		tracked_objects[p_id] = TrackedObject(p_id);
	}
	TrackedObject &tobj = tracked_objects[p_id];
	ObjectID cid = p_config->get_instance_id();
	if (p_config->is_class_ptr(MultiplayerSpawner::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj.spawner.is_valid(), ERR_ALREADY_EXISTS);
		tobj.spawner = cid;
	} else if (p_config->is_class_ptr(MultiplayerSynchronizer::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj.synchronizer.is_valid(), ERR_ALREADY_EXISTS);
		tobj.synchronizer = cid;
	}
	return OK;
}

Error SceneTreeReplicatorInterface::untrack(const ObjectID &p_id, Object *p_config) {
	TrackedObject *tobj = tracked_objects.getptr(p_id);
	ERR_FAIL_COND_V(!tobj, ERR_DOES_NOT_EXIST);
	ObjectID cid = p_config->get_instance_id();
	if (p_config->is_class_ptr(MultiplayerSpawner::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj->spawner != cid, ERR_INVALID_DATA);
		tobj->spawner = ObjectID();
	} else if (p_config->is_class_ptr(MultiplayerSynchronizer::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj->synchronizer != cid, ERR_INVALID_DATA);
		tobj->synchronizer = ObjectID();
	}
	if (tobj->spawner.is_null() && tobj->synchronizer.is_null()) {
		tracked_objects.erase(p_id);
	}
	return OK;
}

Error SceneTreeReplicatorInterface::_send_spawn(const TrackedObject &p_tracked, int p_peer) {
	ERR_FAIL_COND_V(!multiplayer, ERR_BUG);
	Node *node = p_tracked.get_node();
	MultiplayerSpawner *spawner = p_tracked.get_spawner();
	ERR_FAIL_COND_V(!spawner, ERR_BUG);
	ERR_FAIL_COND_V(!node, ERR_BUG);

	ResourceUID::ID scene_id = ResourceUID::INVALID_ID;
	PackedByteArray args;
	if (p_tracked.is_custom()) {
		// Encode custom args
		Array spawn_args = p_tracked.args;
		args.resize(spawn_args.size());
	} else {
		const String scene_path = node->get_scene_file_path();
		ERR_FAIL_COND_V(scene_path.is_empty(), ERR_BUG);
		scene_id = ResourceLoader::get_resource_uid(scene_path);
		ERR_FAIL_COND_V(scene_id == ResourceUID::INVALID_ID, ERR_BUG);
	}

	PackedByteArray packet;

	// Prepare spawn state.
	PackedByteArray state;
	MultiplayerSynchronizer *synchronizer = p_tracked.get_synchronizer();
	if (synchronizer) {
		state = synchronizer->get_replication_config()->encode_spawn_state(node);
	}

	// Prepare simplified path.
	const Node *root_node = multiplayer->get_root_node();
	ERR_FAIL_COND_V(!root_node, ERR_UNCONFIGURED);
	NodePath rel_path = (root_node->get_path()).rel_path_to(spawner->get_path());

	int path_id = 0;
	multiplayer->send_confirm_path(spawner, rel_path, p_peer, path_id);

	// Encode name and parent ID.
	CharString cname = node->get_name().operator String().utf8();
	int nlen = encode_cstring(cname.get_data(), nullptr);
	packet.resize(8 + 4 + 4 + 4 + nlen + state.size());
	int ofs = 0;
	uint8_t *ptr = packet.ptrw();
	ofs += encode_uint64(scene_id, &ptr[ofs]);
	ofs += encode_uint32(path_id, &ptr[ofs]);
	ofs += encode_uint32(p_tracked.net_id.get_id(), &ptr[ofs]);
	ofs += encode_uint32(nlen, &ptr[ofs]);
	ofs += encode_cstring(cname.get_data(), &ptr[ofs]);
	if (state.size()) {
		memcpy(&ptr[ofs], state.ptr(), state.size());
	}
	return send_spawn(packet, p_peer);
}

Error SceneTreeReplicatorInterface::_send_despawn(const TrackedObject &p_tracked, int p_peer) {
	PackedByteArray packet;
	packet.resize(4);
	int ofs = 0;
	uint8_t *ptr = packet.ptrw();
	ofs += encode_uint32(p_tracked.net_id.get_id(), &ptr[ofs]);
	return send_despawn(packet, p_peer);
}

Error SceneTreeReplicatorInterface::_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	ERR_FAIL_COND_V_MSG(p_buffer_len < 21, ERR_INVALID_DATA, "Invalid spawn packet received");
	int ofs = 1; // The spawn/despawn command.
	ResourceUID::ID scene_id = decode_uint64(&p_buffer[ofs]);
	ofs += 8;
	uint32_t node_target = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	uint32_t net_id = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	Node *parent = multiplayer->get_cached_node(p_from, node_target);
	MultiplayerSpawner *spawner = Object::cast_to<MultiplayerSpawner>(parent);
	ERR_FAIL_COND_V(!spawner, ERR_DOES_NOT_EXIST);
	ERR_FAIL_COND_V(!spawner->can_spawn(scene_id), ERR_UNAUTHORIZED);
	ERR_FAIL_COND_V(p_from != spawner->get_multiplayer_authority(), ERR_UNAUTHORIZED);

	uint32_t name_len = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	ERR_FAIL_COND_V_MSG(name_len > uint32_t(p_buffer_len - ofs), ERR_INVALID_DATA, vformat("Invalid spawn packet size: %d, wants: %d", p_buffer_len, ofs + name_len));
	ERR_FAIL_COND_V_MSG(name_len < 1, ERR_INVALID_DATA, "Zero spawn name size.");

	// We need to make sure no trickery happens here, but we want to allow autogenerated ("@") node names.
	const String name = String::utf8((const char *)&p_buffer[ofs], name_len);
	ERR_FAIL_COND_V_MSG(name.validate_node_name() != name.replace("@", ""), ERR_INVALID_DATA, vformat("Invalid node name received: '%s'", name));
	ofs += name_len;

	PackedByteArray state;
	int state_len = p_buffer_len - ofs;
	if (state_len) {
		state.resize(state_len);
		memcpy(state.ptrw(), &p_buffer[ofs], state_len);
	}

	// Spawn the scene.
	String scene_path = ResourceUID::get_singleton()->get_id_path(scene_id);
	RES res = ResourceLoader::load(scene_path);
	ERR_FAIL_COND_V_MSG(!res.is_valid(), ERR_CANT_OPEN, "Unable to load scene to spawn at path: " + scene_path);
	PackedScene *scene = Object::cast_to<PackedScene>(res.ptr());
	ERR_FAIL_COND_V(!scene, ERR_CANT_OPEN);
	Node *node = scene->instantiate();

	spawning_state = &state;
	spawning = node->get_instance_id();
	Error err = spawner->remote_spawn(node, name);
	spawning_state = nullptr;
	spawning = ObjectID();
	if (err == OK) {
		TrackedObject rtobj(TrackedObject(node->get_instance_id(), net_id));
		rtobj.spawner = spawner->get_instance_id();
		remote_objects[NetID(net_id, p_from)] = rtobj;
	} else {
		memdelete(node);
	}
	return err;
}

Error SceneTreeReplicatorInterface::_apply_spawn_state(Object *p_obj, MultiplayerSynchronizer *p_synchronizer) {
	ERR_FAIL_COND_V(!p_obj || !p_synchronizer || !spawning_state || spawning.is_null(), ERR_BUG);
	Error err = p_synchronizer->get_replication_config()->decode_spawn_state(p_obj, *spawning_state);
	spawning_state = nullptr;
	spawning = ObjectID();
	return err;
}

Error SceneTreeReplicatorInterface::_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	ERR_FAIL_COND_V_MSG(p_buffer_len < 5, ERR_INVALID_DATA, "Invalid spawn packet received");
	int ofs = 1; // The spawn/despawn command.
	uint32_t net_id = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	NetID nid(net_id, p_from);
	ERR_FAIL_COND_V(!remote_objects.has(nid), ERR_INVALID_DATA);
	const TrackedObject &tracked = remote_objects[nid];
	Node *node = tracked.get_node();
	MultiplayerSpawner *spawner = tracked.get_spawner();
	ERR_FAIL_COND_V(!node || !spawner, ERR_INVALID_DATA);
	ERR_FAIL_COND_V(p_from != spawner->get_multiplayer_authority(), ERR_UNAUTHORIZED);
	Error err = spawner->remote_despawn(node);
	remote_objects.erase(nid);
	return err;
}

Error SceneTreeReplicatorInterface::on_spawn_send(Object *p_obj, int p_peer) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	ObjectID oid = node->get_instance_id();
	ERR_FAIL_COND_V(!tracked_objects.has(oid), ERR_INVALID_PARAMETER);
	TrackedObject &tobj = tracked_objects[oid];
	if (tobj.net_id.is_null()) {
		tobj.net_id = NetID(++last_net_id);
	}
	return _send_spawn(tobj, p_peer);
}

Error SceneTreeReplicatorInterface::on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	return _spawn_receive(p_from, p_buffer, p_buffer_len);
}

Error SceneTreeReplicatorInterface::on_despawn_send(Object *p_obj, int p_peer) {
	ERR_FAIL_COND_V(!p_obj, ERR_INVALID_PARAMETER);
	ObjectID oid = p_obj->get_instance_id();
	ERR_FAIL_COND_V(!tracked_objects.has(oid), ERR_INVALID_PARAMETER);
	TrackedObject &tracked = tracked_objects[oid];
	return _send_despawn(tracked, p_peer);
}

Error SceneTreeReplicatorInterface::on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	return _despawn_receive(p_from, p_buffer, p_buffer_len);
}

Error SceneTreeReplicatorInterface::on_sync_send(Object *p_obj, int p_peer) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	return OK;
}

Error SceneTreeReplicatorInterface::on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	return OK;
}

SceneTreeReplicatorInterface::SceneTreeReplicatorInterface() {
}
