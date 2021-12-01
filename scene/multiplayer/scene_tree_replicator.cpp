#include "scene_tree_replicator.h"

#include "core/io/marshalls.h"
#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/node.h"
#include "scene/multiplayer/multiplayer_spawner.h"
#include "scene/multiplayer/multiplayer_synchronizer.h"
#include "scene/scene_string_names.h"

#define MAKE_ROOM(m_amount)             \
	if (packet_cache.size() < m_amount) \
		packet_cache.resize(m_amount);

MultiplayerReplicationInterface *SceneTreeReplicatorInterface::_create() {
	return memnew(SceneTreeReplicatorInterface);
}

void SceneTreeReplicatorInterface::make_default() {
	MultiplayerAPI::create_default_replication_interface = _create;
}

SceneTreeReplicatorInterface::TrackedNode &SceneTreeReplicatorInterface::_track(const ObjectID &p_id) {
	if (!tracked_nodes.has(p_id)) {
		tracked_nodes[p_id] = TrackedNode(p_id);
		Node *node = Object::cast_to<Node>(ObjectDB::get_instance(p_id));
		node->connect(SceneStringNames::get_singleton()->tree_exited, callable_mp(this, &SceneTreeReplicatorInterface::_untrack), varray(p_id), Node::CONNECT_ONESHOT);
	}
	return tracked_nodes[p_id];
}

void SceneTreeReplicatorInterface::_untrack(const ObjectID &p_id) {
	if (tracked_nodes.has(p_id)) {
		const NetID &nid = tracked_nodes[p_id].net_id;
		if (peers_info.has(nid.get_peer())) {
			peers_info[nid.get_peer()].recv_nodes.erase(nid);
		}
		tracked_nodes.erase(p_id);
	}
}

bool SceneTreeReplicatorInterface::has_authority(const TrackedNode &p_tracked) const {
	MultiplayerSpawner *spawner = p_tracked.get_spawner();
	ERR_FAIL_COND_V(!spawner || !spawner->is_inside_tree(), false);
	return spawner->get_multiplayer()->has_multiplayer_peer() ? spawner->is_multiplayer_authority() : false;
}

SceneTreeReplicatorInterface::TrackedNode *SceneTreeReplicatorInterface::get_remote(const NetID &p_id) {
	ERR_FAIL_COND_V(!peers_info.has(p_id.get_peer()), nullptr);
	const PeerInfo &info = peers_info[p_id.get_peer()];
	ERR_FAIL_COND_V(!info.recv_nodes.has(p_id), nullptr);
	const ObjectID &oid = info.recv_nodes[p_id];
	return tracked_nodes.getptr(oid);
}

void SceneTreeReplicatorInterface::_free_remotes(PeerInfo *p_info) {
	ERR_FAIL_COND(!p_info);
	HashMap<NetID, ObjectID> &remotes = p_info->recv_nodes;
	const NetID *k = nullptr;
	while ((k = remotes.next(k))) {
		const ObjectID oid = remotes.get(*k);
		ERR_CONTINUE(!tracked_nodes.has(oid));
		tracked_nodes[oid].get_node()->queue_delete();
	}
}

void SceneTreeReplicatorInterface::on_peer_change(int p_id, bool p_connected) {
	if (p_connected) {
		peers_info[p_id] = PeerInfo();
		for (const ObjectID &oid : spawned_nodes) {
			ERR_CONTINUE(!is_tracked(oid));
			TrackedNode &tobj = _track(oid);
			if (has_authority(tobj)) {
				_send_spawn(tobj, p_id);
			}
		}
	} else {
		ERR_FAIL_COND(!peers_info.has(p_id));
		_free_remotes(peers_info.getptr(p_id));
		peers_info.erase(p_id);
	}
}

void SceneTreeReplicatorInterface::on_reset() {
	const int *k = nullptr;
	while ((k = peers_info.next(k))) {
		_free_remotes(peers_info.getptr(*k));
	}
	peers_info.clear();
	// Tracked nodes are cleared on deletion, here we only reset the ids so they can be later re-assigned.
	const ObjectID *oid = nullptr;
	while ((oid = tracked_nodes.next(oid))) {
		TrackedNode &tobj = tracked_nodes[*oid];
		tobj.net_id = NetID();
		tobj.last_sync = 0;
	}
}

void SceneTreeReplicatorInterface::on_network_process() {
	const int *pid = nullptr;
	while ((pid = peers_info.next(pid))) {
		_send_sync(peers_info.get(*pid), *pid);
	}
}

Error SceneTreeReplicatorInterface::on_spawn(Object *p_obj, Variant p_config) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node || p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	MultiplayerSpawner *spawner = Object::cast_to<MultiplayerSpawner>(p_config.get_validated_object());
	ERR_FAIL_COND_V(!spawner, ERR_INVALID_PARAMETER);
	const ObjectID oid = node->get_instance_id();
	const ObjectID sid = spawner->get_instance_id();
	TrackedNode &tobj = _track(oid);
	ERR_FAIL_COND_V(tobj.spawner != ObjectID(), ERR_ALREADY_IN_USE);
	tobj.spawner = sid;
	spawned_nodes.insert(oid);
	_send_spawn(tobj, 0);
	return OK;
}

Error SceneTreeReplicatorInterface::on_despawn(Object *p_obj, Variant p_config) {
	ERR_FAIL_COND_V(p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	MultiplayerSpawner *spawner = Object::cast_to<MultiplayerSpawner>(p_config.get_validated_object());
	ERR_FAIL_COND_V(!p_obj || !spawner, ERR_INVALID_PARAMETER);
	const ObjectID oid = p_obj->get_instance_id();
	const ObjectID sid = spawner->get_instance_id();
	ERR_FAIL_COND_V(!is_tracked(oid), ERR_INVALID_PARAMETER);
	TrackedNode &tobj = _track(oid);
	ERR_FAIL_COND_V(tobj.spawner != sid, ERR_INVALID_PARAMETER);
	spawned_nodes.erase(oid);
	_send_despawn(tobj, 0);
	tobj.spawner = ObjectID();
	return OK;
}

Error SceneTreeReplicatorInterface::on_replication_start(Object *p_obj, Variant p_config) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node || p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	MultiplayerSynchronizer *sync = Object::cast_to<MultiplayerSynchronizer>(p_config.get_validated_object());
	ERR_FAIL_COND_V(!sync, ERR_INVALID_PARAMETER);

	const ObjectID oid = node->get_instance_id();
	const ObjectID sid = sync->get_instance_id();
	TrackedNode &tobj = _track(oid);
	ERR_FAIL_COND_V(tobj.synchronizer != ObjectID(), ERR_ALREADY_IN_USE);
	tobj.synchronizer = sid;
	if (tobj.spawn_state) {
		// Apply spawn state for remotely spawned node before ready.
		const List<NodePath> props = sync->get_replication_config()->get_spawn_properties();
		Vector<Variant> vars;
		vars.resize(props.size());
		int consumed;
		Error err = MultiplayerAPI::decode_and_decompress_variants(vars, tobj.spawn_state, tobj.spawn_state_size, consumed);
		tobj.spawn_state = nullptr;
		tobj.spawn_state_size = 0;
		ERR_FAIL_COND_V(err, err);
		return SceneReplicationConfig::set_state(props, p_obj, vars);
	}
	return OK;
}

Error SceneTreeReplicatorInterface::on_replication_stop(Object *p_obj, Variant p_config) {
	ERR_FAIL_COND_V(p_config.get_type() != Variant::OBJECT, ERR_INVALID_PARAMETER);
	Node *sync = Object::cast_to<MultiplayerSynchronizer>(p_config.get_validated_object());
	ERR_FAIL_COND_V(!p_obj || !sync, ERR_INVALID_PARAMETER);
	const ObjectID oid = p_obj->get_instance_id();
	const ObjectID sid = sync->get_instance_id();
	ERR_FAIL_COND_V(!is_tracked(oid), ERR_INVALID_PARAMETER);
	TrackedNode &tobj = _track(oid);
	ERR_FAIL_COND_V(tobj.synchronizer != sid, ERR_INVALID_PARAMETER);
	tobj.synchronizer = ObjectID();
	return OK;
}

Error SceneTreeReplicatorInterface::_send_spawn(TrackedNode &p_tracked, int p_peer) {
	ERR_FAIL_COND_V(p_peer < 0, ERR_BUG);
	ERR_FAIL_COND_V(!multiplayer, ERR_BUG);
	Node *node = p_tracked.get_node();
	MultiplayerSpawner *spawner = p_tracked.get_spawner();
	ERR_FAIL_COND_V(!spawner, ERR_BUG);
	ERR_FAIL_COND_V(!node, ERR_BUG);

	if (p_tracked.net_id.is_null()) {
		p_tracked.net_id = NetID(++last_net_id);
	}
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

	// Prepare spawn state.
	int state_size = 0;
	Vector<Variant> state_vars;
	Vector<const Variant *> state_varp;
	MultiplayerSynchronizer *synchronizer = p_tracked.get_synchronizer();
	if (synchronizer && synchronizer->get_replication_config().is_valid()) {
		const List<NodePath> props = synchronizer->get_replication_config()->get_spawn_properties();
		Error err = SceneReplicationConfig::get_state(props, node, state_vars, state_varp);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Unable to retrieve spawn state.");
		err = MultiplayerAPI::encode_and_compress_variants(state_varp.ptrw(), state_varp.size(), nullptr, state_size);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Unable to encode spawn state.");
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
	MAKE_ROOM(1 + 8 + 4 + 4 + 4 + nlen + (is_custom ? 4 + spawn_arg_size : 0) + state_size);
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
	if (state_size) {
		Error err = MultiplayerAPI::encode_and_compress_variants(state_varp.ptrw(), state_varp.size(), &ptr[ofs], state_size);
		ERR_FAIL_COND_V(err, err);
		ofs += state_size;
	}
	Error err = send_raw(ptr, ofs, p_peer, Multiplayer::TRANSFER_MODE_RELIABLE, 0);
	if (p_peer) {
		ERR_FAIL_COND_V(!peers_info.has(p_peer), ERR_BUG);
		peers_info[p_peer].sent_nodes.insert(p_tracked.id);
	} else {
		const int *pid = nullptr;
		while ((pid = peers_info.next(pid))) {
			peers_info.get(*pid).sent_nodes.insert(p_tracked.id);
		}
	}
	return err;
}

Error SceneTreeReplicatorInterface::_send_despawn(const TrackedNode &p_tracked, int p_peer) {
	MAKE_ROOM(5);
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = (uint8_t)MultiplayerAPI::NETWORK_COMMAND_DESPAWN;
	int ofs = 1;
	ofs += encode_uint32(p_tracked.net_id.get_id(), &ptr[ofs]);
	Error err = send_raw(ptr, ofs, p_peer, Multiplayer::TRANSFER_MODE_RELIABLE, 0);
	if (p_peer) {
		ERR_FAIL_COND_V(!peers_info.has(p_peer), ERR_BUG);
		peers_info[p_peer].sent_nodes.erase(p_tracked.id);
	} else {
		const int *pid = nullptr;
		while ((pid = peers_info.next(pid))) {
			peers_info.get(*pid).sent_nodes.erase(p_tracked.id);
		}
	}
	return err;
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

	// Track object.
	NetID nid(net_id, p_from);
	TrackedNode &tobj = _track(oid);
	tobj.spawner = spawner->get_instance_id();
	tobj.net_id = nid;

	// Store the temporary state buffer so that it can be applied by the synchronizer notification when entering tree.
	int state_len = p_buffer_len - ofs;
	if (state_len) {
		tobj.spawn_state = &p_buffer[ofs];
		tobj.spawn_state_size = state_len;
	}
	parent->add_child(node);
	tobj.spawn_state = nullptr;
	tobj.spawn_state_size = 0;

	// Also track as a remote.
	peers_info[p_from].recv_nodes[nid] = oid;
	return OK;
}

Error SceneTreeReplicatorInterface::on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	ERR_FAIL_COND_V_MSG(p_buffer_len < 5, ERR_INVALID_DATA, "Invalid spawn packet received");
	int ofs = 1; // The spawn/despawn command.
	uint32_t net_id = decode_uint32(&p_buffer[ofs]);
	ofs += 4;
	NetID nid(net_id, p_from);
	const TrackedNode *tracked = get_remote(nid);
	ERR_FAIL_COND_V(!tracked, ERR_INVALID_DATA);
	Node *node = tracked->get_node();
	MultiplayerSpawner *spawner = tracked->get_spawner();
	ERR_FAIL_COND_V(!node || !spawner, ERR_INVALID_DATA);
	ERR_FAIL_COND_V(p_from != spawner->get_multiplayer_authority(), ERR_UNAUTHORIZED);
	node->queue_delete();
	return OK;
}

void SceneTreeReplicatorInterface::_send_sync(PeerInfo &p_info, int p_peer) {
	if (p_info.sent_nodes.is_empty()) {
		return;
	}
	MAKE_ROOM(sync_mtu);
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = MultiplayerAPI::NETWORK_COMMAND_SYNC;
	int ofs = 1;
	ofs += encode_uint16(++p_info.last_sent_sync, &ptr[1]);
	// Can only send updates for already notified nodes.
	// This is a lazy implementation, we could optimize much more here with by grouping by replication config.
	for (const ObjectID &oid : p_info.sent_nodes) {
		ERR_CONTINUE(!is_tracked(oid));
		TrackedNode &tobj = tracked_nodes[oid];
		ERR_CONTINUE(tobj.net_id.is_null());
		MultiplayerSynchronizer *sync = tobj.get_synchronizer();
		if (!sync) {
			continue; // nothing to sync.
		}
		Node *node = tobj.get_node();
		ERR_CONTINUE(!node);
		int size;
		Vector<Variant> vars;
		Vector<const Variant *> varp;
		const List<NodePath> props = sync->get_replication_config()->get_sync_properties();
		Error err = SceneReplicationConfig::get_state(props, node, vars, varp);
		ERR_CONTINUE_MSG(err != OK, "Unable to retrieve sync state.");
		err = MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), nullptr, size);
		ERR_CONTINUE_MSG(err != OK, "Unable to encode sync state.");
		// TODO Handle single state above MTU.
		ERR_CONTINUE_MSG(size > 3 + 4 + 4 + sync_mtu, vformat("Node states bigger then MTU will not be sent (%d > %d): %s", size, sync_mtu, node->get_path()));
		if (ofs + 4 + 4 + size > sync_mtu) {
			// Send what we got, and reset write.
			send_raw(packet_cache.ptr(), ofs, p_peer, Multiplayer::TRANSFER_MODE_UNRELIABLE, 0);
			ofs = 3;
		}
		if (size) {
			ofs += encode_uint32(tobj.net_id.get_id(), &ptr[ofs]);
			ofs += encode_uint32(size, &ptr[ofs]);
			MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), &ptr[ofs], size);
			ofs += size;
		}
	}
	if (ofs > 3) {
		// Got some left over to send.
		send_raw(packet_cache.ptr(), ofs, p_peer, Multiplayer::TRANSFER_MODE_UNRELIABLE, 0);
	}
}

Error SceneTreeReplicatorInterface::on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	ERR_FAIL_COND_V(!peers_info.has(p_from), ERR_BUG);
	ERR_FAIL_COND_V_MSG(p_buffer_len < 11, ERR_INVALID_DATA, "Invalid sync packet received");
	uint16_t time = decode_uint16(&p_buffer[1]);
	int ofs = 3;
	while (ofs + 8 < p_buffer_len) {
		uint32_t net_id = decode_uint32(&p_buffer[ofs]);
		ofs += 4;
		uint32_t size = decode_uint32(&p_buffer[ofs]);
		ofs += 4;
		TrackedNode *tobj = get_remote(NetID(net_id, p_from));
		if (!tobj) {
			// Not received yet.
			ofs += size;
			continue;
		}
		if (time <= tobj->last_sync && tobj->last_sync - time < 32767) {
			ofs += size;
			continue;
		}
		Node *node = tobj->get_node();
		MultiplayerSynchronizer *sync = tobj->get_synchronizer();
		ERR_FAIL_COND_V(!node || !sync, ERR_BUG);
		ERR_FAIL_COND_V(size > uint32_t(p_buffer_len - ofs), ERR_BUG);
		const List<NodePath> props = sync->get_replication_config()->get_sync_properties();
		Vector<Variant> vars;
		vars.resize(props.size());
		int consumed;
		Error err = MultiplayerAPI::decode_and_decompress_variants(vars, &p_buffer[ofs], size, consumed);
		ERR_FAIL_COND_V(err, err);
		err = SceneReplicationConfig::set_state(props, node, vars);
		ERR_FAIL_COND_V(err, err);
		ofs += size;
		tobj->last_sync = time;
	}
	return OK;
}

SceneTreeReplicatorInterface::SceneTreeReplicatorInterface() {
}
