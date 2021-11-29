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

void SceneTreeReplicatorInterface::on_network_process() {
}

bool SceneTreeReplicatorInterface::has_authority(const TrackedObject &p_tracked) const {
	MultiplayerSpawner *spawner = p_tracked.get_spawner();
	ERR_FAIL_COND_V(!spawner || !spawner->is_inside_tree(), false);
	return spawner->get_multiplayer()->has_multiplayer_peer() ? spawner->is_multiplayer_authority() : false;
}

Error SceneTreeReplicatorInterface::on_replication_start(Object *p_obj, Variant p_config) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node || p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	Node *config = Object::cast_to<Node>(p_config.get_validated_object());
	ERR_FAIL_COND_V(!config, ERR_INVALID_PARAMETER);

	ObjectID oid = node->get_instance_id();
	if (!tracked_objects.has(oid)) {
		tracked_objects[oid] = TrackedObject(oid);
	}
	TrackedObject &tobj = tracked_objects[oid];

	ObjectID cid = config->get_instance_id();
	if (config->is_class_ptr(MultiplayerSpawner::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj.spawner != ObjectID(), ERR_ALREADY_IN_USE);
		tobj.spawner = cid;
		if (has_authority(tobj)) {
			tobj.net_id = NetID(++last_net_id);
			_send_spawn(tobj, 0);
		}
		return OK;
	} else if (config->is_class_ptr(MultiplayerSynchronizer::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj.synchronizer != ObjectID(), ERR_ALREADY_IN_USE);
		tobj.synchronizer = cid;
		if (is_spawning(p_obj)) {
			// Apply spawn state for remotely spawned node before ready.
			return ((MultiplayerSynchronizer *)config)->get_replication_config()->decode_spawn_state(p_obj, *spawning_state);
		}
		return OK;
	}
	tracked_objects.erase(oid);
	ERR_FAIL_V(ERR_INVALID_PARAMETER);
}

Error SceneTreeReplicatorInterface::on_replication_stop(Object *p_obj, Variant p_config) {
	ERR_FAIL_COND_V(p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	Node *config = Object::cast_to<Node>(p_config.get_validated_object());
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!config, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	ObjectID oid = node->get_instance_id();
	ERR_FAIL_COND_V(!tracked_objects.has(oid), ERR_INVALID_PARAMETER);

	TrackedObject &tobj = tracked_objects[oid];
	const ObjectID cid = config->get_instance_id();
	if (config->is_class_ptr(MultiplayerSpawner::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj.spawner != cid, ERR_INVALID_PARAMETER);
		if (has_authority(tobj) && !tobj.spawn_pending) {
			_send_despawn(tobj, 0);
		}
		tobj.spawner = ObjectID();
	} else if (config->is_class_ptr(MultiplayerSynchronizer::get_class_ptr_static())) {
		ERR_FAIL_COND_V(tobj.synchronizer != cid, ERR_INVALID_PARAMETER);
		tobj.synchronizer = ObjectID();
	} else {
		return ERR_INVALID_PARAMETER;
	}
	if (tobj.spawner.is_null() && tobj.synchronizer.is_null()) {
		tracked_objects.erase(oid);
	}
	return OK;
}

#define MAKE_ROOM(m_amount)             \
	if (packet_cache.size() < m_amount) \
		packet_cache.resize(m_amount);

Error SceneTreeReplicatorInterface::_send_spawn(const TrackedObject &p_tracked, int p_peer) {
	ERR_FAIL_COND_V(!multiplayer, ERR_BUG);
	Node *node = p_tracked.get_node();
	MultiplayerSpawner *spawner = p_tracked.get_spawner();
	ERR_FAIL_COND_V(!spawner, ERR_BUG);
	ERR_FAIL_COND_V(!node, ERR_BUG);

	ResourceUID::ID scene_id = ResourceUID::INVALID_ID;

	// Prepare custom arg and scene_id
	Variant spawn_arg = spawner->get_spawn_argument(p_tracked.id);
	int spawn_arg_size = 0;
	if (spawn_arg.get_type() != Variant::NIL) {
		Error err = MultiplayerAPI::encode_and_compress_variant(spawn_arg, nullptr, spawn_arg_size, false);
		ERR_FAIL_COND_V(err, err);
	} else {
		const String scene_path = node->get_scene_file_path();
		ERR_FAIL_COND_V(scene_path.is_empty(), ERR_BUG);
		scene_id = ResourceLoader::get_resource_uid(scene_path);
		ERR_FAIL_COND_V(scene_id == ResourceUID::INVALID_ID, ERR_BUG);
	}
	bool is_custom = scene_id == ResourceUID::INVALID_ID;

	// Prepare spawn state. TODO should do better
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
	MAKE_ROOM(1 + 8 + 4 + 4 + 4 + nlen + (is_custom ? 4 + spawn_arg_size : 0) + state.size());
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = (uint8_t)MultiplayerAPI::NETWORK_COMMAND_SPAWN;
	int ofs = 1;
	ofs += encode_uint64(scene_id, &ptr[ofs]);
	ofs += encode_uint32(path_id, &ptr[ofs]);
	ofs += encode_uint32(p_tracked.net_id.get_id(), &ptr[ofs]);
	ofs += encode_uint32(nlen, &ptr[ofs]);
	ofs += encode_cstring(cname.get_data(), &ptr[ofs]);
	// Write args
	if (is_custom) {
		ofs += encode_uint32(spawn_arg_size, &ptr[ofs]);
		Error err = MultiplayerAPI::encode_and_compress_variant(spawn_arg, &ptr[ofs], spawn_arg_size, false);
		ERR_FAIL_COND_V(err, err);
		ofs += spawn_arg_size;
	}
	// Write state.
	if (state.size()) {
		memcpy(&ptr[ofs], state.ptr(), state.size());
	}
	ofs += state.size();
	return send_raw(ptr, ofs, p_peer, Multiplayer::TRANSFER_MODE_RELIABLE, 0);
}

Error SceneTreeReplicatorInterface::_send_despawn(const TrackedObject &p_tracked, int p_peer) {
	MAKE_ROOM(5);
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = (uint8_t)MultiplayerAPI::NETWORK_COMMAND_DESPAWN;
	int ofs = 1;
	ofs += encode_uint32(p_tracked.net_id.get_id(), &ptr[ofs]);
	return send_raw(ptr, ofs, p_peer, Multiplayer::TRANSFER_MODE_RELIABLE, 0);
}

Error SceneTreeReplicatorInterface::on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	ERR_FAIL_COND_V_MSG(p_buffer_len < 21, ERR_INVALID_DATA, "Invalid spawn packet received");
	int ofs = 1; // The spawn/despawn command.
	ResourceUID::ID scene_id = decode_uint64(&p_buffer[ofs]);
	ofs += 8;
	uint32_t node_target = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	MultiplayerSpawner *spawner = Object::cast_to<MultiplayerSpawner>(multiplayer->get_cached_node(p_from, node_target));
	ERR_FAIL_COND_V(!spawner, ERR_DOES_NOT_EXIST);
	ERR_FAIL_COND_V(p_from != spawner->get_multiplayer_authority(), ERR_UNAUTHORIZED);

	uint32_t net_id = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	uint32_t name_len = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	ERR_FAIL_COND_V_MSG(name_len > uint32_t(p_buffer_len - ofs), ERR_INVALID_DATA, vformat("Invalid spawn packet size: %d, wants: %d", p_buffer_len, ofs + name_len));
	ERR_FAIL_COND_V_MSG(name_len < 1, ERR_INVALID_DATA, "Zero spawn name size.");

	// We need to make sure no trickery happens here, but we want to allow autogenerated ("@") node names.
	const String name = String::utf8((const char *)&p_buffer[ofs], name_len);
	ERR_FAIL_COND_V_MSG(name.validate_node_name() != name, ERR_INVALID_DATA, vformat("Invalid node name received: '%s'. Make sure to add nodes via 'add_child(node, true)' remotely.", name));
	ofs += name_len;

	// Check that we can spawn.
	Node *parent = spawner->get_node_or_null(spawner->get_spawn_path());
	ERR_FAIL_COND_V(!parent, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(parent->has_node(name), ERR_INVALID_DATA);

	Node *node = nullptr;
	if (scene_id == ResourceUID::INVALID_ID) {
		// Custom spawn.
		ERR_FAIL_COND_V(p_buffer_len - ofs < 4, ERR_INVALID_DATA);
		uint32_t arg_size = decode_uint32(&p_buffer[ofs]);
		ofs += 4;
		ERR_FAIL_COND_V(arg_size > uint32_t(p_buffer_len - ofs), ERR_INVALID_DATA);
		Variant v;
		Error err = MultiplayerAPI::decode_and_decompress_variant(v, &p_buffer[ofs], arg_size, nullptr, false);
		ERR_FAIL_COND_V(err != OK, err);
		ofs += arg_size;
		node = spawner->instantiate_custom(v);
	} else {
		// Scene based spawn.
		const String scene_path = ResourceUID::get_singleton()->get_id_path(scene_id);
		ERR_FAIL_COND_V(!spawner->can_spawn_scene(scene_path), ERR_UNAUTHORIZED);
		RES res = ResourceLoader::load(scene_path);
		ERR_FAIL_COND_V_MSG(!res.is_valid(), ERR_DOES_NOT_EXIST, "Unable to load scene to spawn at path: " + scene_path);
		PackedScene *scene = Object::cast_to<PackedScene>(res.ptr());
		ERR_FAIL_COND_V_MSG(!scene, ERR_BUG, "Failed to cast resource to scene, probably a bug");
		node = scene->instantiate();
	}
	ERR_FAIL_COND_V(!node, ERR_UNAUTHORIZED);
	node->set_name(name);

	ObjectID oid = node->get_instance_id();

	// Make sure to apply the state during the enter tree notification if
	// a synchronizer is available for that node.
	PackedByteArray state;
	int state_len = p_buffer_len - ofs;
	if (state_len) {
		state.resize(state_len);
		memcpy(state.ptrw(), &p_buffer[ofs], state_len);
	}
	spawning = oid;
	spawning_state = &state;
	parent->add_child(node);
	spawning = ObjectID();
	spawning_state = nullptr;

	// Track as remote.
	TrackedObject rtobj(TrackedObject(oid, net_id));
	rtobj.spawner = spawner->get_instance_id();
	remote_objects[NetID(net_id, p_from)] = rtobj;

	return OK;
}

Error SceneTreeReplicatorInterface::on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
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
	node->queue_delete();
	remote_objects.erase(nid);
	return OK;
}

Error SceneTreeReplicatorInterface::on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	return OK;
}

SceneTreeReplicatorInterface::SceneTreeReplicatorInterface() {
}
