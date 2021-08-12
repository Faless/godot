/*************************************************************************/
/*  multiplayer_replicator.cpp                                           */
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

#include "core/io/multiplayer_replicator.h"

#include "core/io/marshalls.h"
#include "scene/main/node.h"
#include "scene/resources/packed_scene.h"

#define MAKE_ROOM(m_amount)             \
	if (packet_cache.size() < m_amount) \
		packet_cache.resize(m_amount);

Error MultiplayerReplicator::_send_default_spawn_despawn(int p_peer_id, const ResourceUID::ID &p_scene_id, const PackedByteArray &p_data, const NodePath &p_path, bool p_spawn) {
	ERR_FAIL_COND_V(!replications.has(p_scene_id), ERR_INVALID_PARAMETER);
	int ofs = 0;

	// Prepare simplified path
	const Node *root_node = multiplayer->get_root_node();
	ERR_FAIL_COND_V(!root_node, ERR_UNCONFIGURED);
	NodePath rel_path = (root_node->get_path()).rel_path_to(p_path);
	const Vector<StringName> names = rel_path.get_names();
	ERR_FAIL_COND_V(names.size() < 2, ERR_INVALID_PARAMETER);

	NodePath parent = NodePath(names.subarray(0, names.size() - 2), false);
	ERR_FAIL_COND_V_MSG(!root_node->has_node(parent), ERR_INVALID_PARAMETER, "Path not found: " + parent);

	int path_id = 0;
	multiplayer->send_confirm_path(root_node->get_node(parent), parent, p_peer_id, path_id);

	// Encode name and parent ID.
	CharString cname = String(names[names.size() - 1]).utf8();
	int nlen = encode_cstring(cname.get_data(), nullptr);
	MAKE_ROOM(SPAWN_CMD_OFFSET + 4 + 4 + nlen + p_data.size());
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = (p_spawn ? MultiplayerAPI::NETWORK_COMMAND_SPAWN : MultiplayerAPI::NETWORK_COMMAND_DESPAWN);
	ofs = 1;
	ofs += encode_uint64(p_scene_id, &ptr[ofs]);
	ofs += encode_uint32(path_id, &ptr[ofs]);
	ofs += encode_uint32(nlen, &ptr[ofs]);
	ofs += encode_cstring(cname.get_data(), &ptr[ofs]);

	if (p_data.size()) {
		memcpy(&ptr[ofs], p_data.ptr(), p_data.size());
	}

	Ref<MultiplayerPeer> network_peer = multiplayer->get_network_peer();
	network_peer->set_target_peer(p_peer_id);
	network_peer->set_transfer_channel(0);
	network_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
	return network_peer->put_packet(ptr, ofs + p_data.size());
}

void MultiplayerReplicator::_process_default_spawn_despawn(int p_from, const ResourceUID::ID &p_scene_id, const uint8_t *p_packet, int p_packet_len, bool p_spawn) {
	ERR_FAIL_COND_MSG(p_packet_len < SPAWN_CMD_OFFSET + 9, "Invalid spawn packet received");
	int ofs = SPAWN_CMD_OFFSET;
	uint32_t node_target = decode_uint32(&p_packet[ofs]);
	Node *parent = multiplayer->get_cached_node(p_from, node_target);
	ofs += 4;
	ERR_FAIL_COND_MSG(parent == nullptr, "Invalid packet received. Requested node was not found.");

	uint32_t name_len = decode_uint32(&p_packet[ofs]);
	ofs += 4;
	ERR_FAIL_COND_MSG(name_len > uint32_t(p_packet_len - ofs), vformat("Invalid spawn packet size: %d, wants: %d", p_packet_len, ofs + name_len));
	ERR_FAIL_COND_MSG(name_len < 1, "Zero spawn name size.");

	const String name = String::utf8((const char *)&p_packet[ofs], name_len);
	// We need to make sure no trickery happens here (e.g. despawning a subpath), but we want to allow autogenerated ("@") node names.
	ERR_FAIL_COND_MSG(name.validate_node_name() != name.replace("@", ""), vformat("Invalid node name received: '%s'", name));
	ofs += name_len;

	const SceneConfig &cfg = replications[p_scene_id];
	if (cfg.mode == REPLICATION_MODE_SERVER && p_from == 1) {
		String scene_path = ResourceUID::get_singleton()->get_id_path(p_scene_id);
		if (p_spawn) {
			ERR_FAIL_COND_MSG(parent->has_node(name), vformat("Unable to spawn node. Node already exists: %s/%s", parent->get_path(), name));
			RES res = ResourceLoader::load(scene_path);
			ERR_FAIL_COND_MSG(!res.is_valid(), "Unable to load scene to spawn at path: " + scene_path);
			PackedScene *scene = Object::cast_to<PackedScene>(res.ptr());
			ERR_FAIL_COND(!scene);
			Node *node = scene->instantiate();
			ERR_FAIL_COND(!node);
			replicated_nodes[node->get_instance_id()] = p_scene_id;
			parent->_add_child_nocheck(node, name);
			emit_signal(SNAME("spawned"), p_scene_id, node);
		} else {
			ERR_FAIL_COND_MSG(!parent->has_node(name), vformat("Path not found: %s/%s", parent->get_path(), name));
			Node *node = parent->get_node(name);
			ERR_FAIL_COND_MSG(!replicated_nodes.has(node->get_instance_id()), vformat("Trying to despawn a Node that was not replicated: %s/%s", parent->get_path(), name));
			emit_signal(SNAME("despawned"), p_scene_id, node);
			replicated_nodes.erase(node->get_instance_id());
			node->queue_delete();
		}
	} else {
		PackedByteArray data;
		if (p_packet_len > ofs) {
			data.resize(p_packet_len - ofs);
			memcpy(data.ptrw(), &p_packet[ofs], data.size());
		}
		if (p_spawn) {
			emit_signal(SNAME("spawn_requested"), p_from, p_scene_id, parent, name, data);
		} else {
			emit_signal(SNAME("despawn_requested"), p_from, p_scene_id, parent, name, data);
		}
	}
}

void MultiplayerReplicator::process_spawn_despawn(int p_from, const uint8_t *p_packet, int p_packet_len, bool p_spawn) {
	ERR_FAIL_COND_MSG(p_packet_len < SPAWN_CMD_OFFSET, "Invalid spawn packet received");
	ResourceUID::ID id = decode_uint64(&p_packet[1]);
	ERR_FAIL_COND_MSG(!replications.has(id), "Invalid spawn ID received " + itos(id));

	const SceneConfig &cfg = replications[id];
	if (cfg.on_spawn_despawn_receive.is_valid()) {
		int ofs = SPAWN_CMD_OFFSET;
		bool is_raw = ((p_packet[0] & 64) >> MultiplayerAPI::BYTE_ONLY_OR_NO_ARGS_SHIFT) == 1;
		Variant data;
		int left = p_packet_len - ofs;
		if (is_raw && left) {
			PackedByteArray pba;
			pba.resize(left);
			memcpy(pba.ptrw(), &p_packet[ofs], pba.size());
			data = pba;
		} else if (left) {
			ERR_FAIL_COND(decode_variant(data, &p_packet[ofs], left) != OK);
		}

		Variant args[4];
		args[0] = p_from;
		args[1] = id;
		args[2] = data;
		args[3] = p_spawn;
		const Variant *argp[] = { &args[0], &args[1], &args[2], &args[3] };
		Callable::CallError ce;
		Variant ret;
		cfg.on_spawn_despawn_receive.call(argp, 4, ret, ce);
		ERR_FAIL_COND_MSG(ce.error != Callable::CallError::CALL_OK, "Custom receive function failed");
	} else {
		_process_default_spawn_despawn(p_from, id, p_packet, p_packet_len, p_spawn);
	}
}

Error MultiplayerReplicator::spawn_config(const ResourceUID::ID &p_id, ReplicationMode p_mode, const Callable &p_on_send, const Callable &p_on_recv) {
	ERR_FAIL_COND_V(p_mode < REPLICATION_MODE_NONE || p_mode > REPLICATION_MODE_CUSTOM, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!ResourceUID::get_singleton()->has_id(p_id), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V_MSG(p_on_send.is_valid() != p_on_recv.is_valid(), ERR_INVALID_PARAMETER, "Send and receive custom callables must be both valid or both empty");
#ifdef TOOLS_ENABLED
	if (!p_on_send.is_valid()) {
		// We allow non scene spawning with custom callables.
		String path = ResourceUID::get_singleton()->get_id_path(p_id);
		RES res = ResourceLoader::load(path);
		ERR_FAIL_COND_V(!res->is_class("PackedScene"), ERR_INVALID_PARAMETER);
	}
#endif
	if (p_mode == REPLICATION_MODE_NONE) {
		if (replications.has(p_id)) {
			replications.erase(p_id);
		}
	} else {
		SceneConfig cfg;
		cfg.mode = p_mode;
		cfg.on_spawn_despawn_send = p_on_send;
		cfg.on_spawn_despawn_receive = p_on_recv;
		replications[p_id] = cfg;
	}
	return OK;
}

Error MultiplayerReplicator::_send_spawn_despawn(int p_peer_id, const ResourceUID::ID &p_scene_id, const Variant &p_data, bool p_spawn) {
	int data_size = 0;
	int is_raw = false;
	if (p_data.get_type() == Variant::PACKED_BYTE_ARRAY) {
		const PackedByteArray pba = p_data;
		is_raw = true;
		data_size = p_data.operator PackedByteArray().size();
	} else if (p_data.get_type() == Variant::NIL) {
		is_raw = true;
	} else {
		Error err = encode_variant(p_data, nullptr, data_size);
		ERR_FAIL_COND_V(err, err);
	}
	MAKE_ROOM(SPAWN_CMD_OFFSET + data_size);
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = (p_spawn ? MultiplayerAPI::NETWORK_COMMAND_SPAWN : MultiplayerAPI::NETWORK_COMMAND_DESPAWN) + ((is_raw ? 1 : 0) << MultiplayerAPI::BYTE_ONLY_OR_NO_ARGS_SHIFT);
	encode_uint64(p_scene_id, &ptr[1]);
	if (p_data.get_type() == Variant::PACKED_BYTE_ARRAY) {
		const PackedByteArray pba = p_data;
		memcpy(&ptr[SPAWN_CMD_OFFSET], pba.ptr(), pba.size());
	} else if (data_size) {
		encode_variant(p_data, &ptr[SPAWN_CMD_OFFSET], data_size);
	}
	Ref<MultiplayerPeer> network_peer = multiplayer->get_network_peer();
	network_peer->set_target_peer(p_peer_id);
	network_peer->set_transfer_channel(0);
	network_peer->set_transfer_mode(MultiplayerPeer::TRANSFER_MODE_RELIABLE);
	return network_peer->put_packet(ptr, SPAWN_CMD_OFFSET + data_size);
}

Error MultiplayerReplicator::send_despawn(int p_peer_id, const ResourceUID::ID &p_scene_id, const Variant &p_data, const NodePath &p_path) {
	ERR_FAIL_COND_V_MSG(!replications.has(p_scene_id), ERR_INVALID_PARAMETER, vformat("Spawnable not found: %d", p_scene_id));
	const SceneConfig &cfg = replications[p_scene_id];
	if (cfg.on_spawn_despawn_send.is_valid()) {
		return _send_spawn_despawn(p_peer_id, p_scene_id, p_data, true);
	} else {
		ERR_FAIL_COND_V_MSG(cfg.mode == REPLICATION_MODE_SERVER && multiplayer->is_network_server(), ERR_UNAVAILABLE, "Manual despawn is restricted in default server mode implementation. Use custom mode if you desire control over server spawn requests.");
		ERR_FAIL_COND_V_MSG(p_path.is_empty(), ERR_INVALID_PARAMETER, "Despawn default implementation requires a despawn path.");
		return _send_default_spawn_despawn(p_peer_id, p_scene_id, p_data, p_path, false);
	}
}

Error MultiplayerReplicator::send_spawn(int p_peer_id, const ResourceUID::ID &p_scene_id, const Variant &p_data, const NodePath &p_path) {
	ERR_FAIL_COND_V_MSG(!replications.has(p_scene_id), ERR_INVALID_PARAMETER, vformat("Spawnable not found: %d", p_scene_id));
	const SceneConfig &cfg = replications[p_scene_id];
	if (cfg.on_spawn_despawn_send.is_valid()) {
		return _send_spawn_despawn(p_peer_id, p_scene_id, p_data, false);
	} else {
		ERR_FAIL_COND_V_MSG(cfg.mode == REPLICATION_MODE_SERVER && multiplayer->is_network_server(), ERR_UNAVAILABLE, "Manual spawn is restricted in default server mode implementation. Use custom mode if you desire control over server spawn requests.");
		ERR_FAIL_COND_V_MSG(p_path.is_empty(), ERR_INVALID_PARAMETER, "Spawn default implementation requires a spawn path.");
		return _send_default_spawn_despawn(p_peer_id, p_scene_id, p_data, p_path, true);
	}
}

Error MultiplayerReplicator::_spawn_despawn(ResourceUID::ID p_scene_id, Object *p_obj, int p_peer, bool p_spawn) {
	ERR_FAIL_COND_V_MSG(!replications.has(p_scene_id), ERR_INVALID_PARAMETER, vformat("Spawnable not found: %d", p_scene_id));

	const SceneConfig &cfg = replications[p_scene_id];
	if (cfg.on_spawn_despawn_send.is_valid()) {
		Variant args[4];
		args[0] = p_peer;
		args[1] = p_scene_id;
		args[2] = p_obj;
		args[3] = true;
		const Variant *argp[] = { &args[0], &args[1], &args[2], &args[3] };
		Callable::CallError ce;
		Variant ret;
		cfg.on_spawn_despawn_send.call(argp, 4, ret, ce);
		ERR_FAIL_COND_V_MSG(ce.error != Callable::CallError::CALL_OK, FAILED, "Custom send function failed");
		return OK;
	} else {
		Node *node = Object::cast_to<Node>(p_obj);
		ERR_FAIL_COND_V_MSG(!p_obj, ERR_INVALID_PARAMETER, "Only nodes can be replicated by the default implementation");
		return _send_default_spawn_despawn(p_peer, p_scene_id, PackedByteArray(), node->get_path(), p_spawn);
	}
}

Error MultiplayerReplicator::spawn(ResourceUID::ID p_scene_id, Object *p_obj, int p_peer) {
	return _spawn_despawn(p_scene_id, p_obj, p_peer, true);
}

Error MultiplayerReplicator::despawn(ResourceUID::ID p_scene_id, Object *p_obj, int p_peer) {
	return _spawn_despawn(p_scene_id, p_obj, p_peer, false);
}

void MultiplayerReplicator::scene_enter_exit_notify(const String &p_scene, Node *p_node, bool p_enter) {
	if (!multiplayer->has_network_peer()) {
		return;
	}
	Node *root_node = multiplayer->get_root_node();
	ERR_FAIL_COND(!p_node || !p_node->get_parent() || !root_node);
	NodePath path = (root_node->get_path()).rel_path_to(p_node->get_parent()->get_path());
	if (path.is_empty()) {
		return;
	}
	ResourceUID::ID id = ResourceLoader::get_resource_uid(p_scene);
	if (!replications.has(id)) {
		return;
	}
	const SceneConfig &cfg = replications[id];
	if (p_enter) {
		if (cfg.mode == REPLICATION_MODE_SERVER && multiplayer->is_network_server()) {
			replicated_nodes[p_node->get_instance_id()] = id;
			spawn(id, p_node, 0);
		}
		emit_signal(SNAME("replicated_instance_added"), id, p_node);
	} else {
		if (cfg.mode == REPLICATION_MODE_SERVER && multiplayer->is_network_server() && replicated_nodes.has(p_node->get_instance_id())) {
			replicated_nodes.erase(p_node->get_instance_id());
			despawn(id, p_node, 0);
		}
		emit_signal(SNAME("replicated_instance_removed"), id, p_node);
	}
}

void MultiplayerReplicator::spawn_all(int p_peer) {
	for (const KeyValue<ObjectID, ResourceUID::ID> &E : replicated_nodes) {
		// Only server mode adds to replicated_nodes, no need to check it.
		Object *obj = ObjectDB::get_instance(E.key);
		ERR_CONTINUE(!obj);
		Node *node = Object::cast_to<Node>(obj);
		ERR_CONTINUE(!node);
		spawn(E.value, node, p_peer);
	}
}

void MultiplayerReplicator::clear() {
	replicated_nodes.clear();
}

void MultiplayerReplicator::_bind_methods() {
	ClassDB::bind_method(D_METHOD("spawn_config", "scene_id", "spawn_mode", "custom_send", "custom_receive"), &MultiplayerReplicator::spawn_config, DEFVAL(Callable()), DEFVAL(Callable()));
	ClassDB::bind_method(D_METHOD("despawn", "scene_id", "object", "peer_id"), &MultiplayerReplicator::despawn, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("spawn", "scene_id", "object", "peer_id"), &MultiplayerReplicator::spawn, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("send_despawn", "peer_id", "scene_id", "data", "path"), &MultiplayerReplicator::send_despawn, DEFVAL(Variant()), DEFVAL(NodePath()));
	ClassDB::bind_method(D_METHOD("send_spawn", "peer_id", "scene_id", "data", "path"), &MultiplayerReplicator::send_spawn, DEFVAL(Variant()), DEFVAL(NodePath()));

	ADD_SIGNAL(MethodInfo("despawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("spawned", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("despawn_requested", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "parent", PROPERTY_HINT_RESOURCE_TYPE, "Node"), PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("spawn_requested", PropertyInfo(Variant::INT, "id"), PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "parent", PROPERTY_HINT_RESOURCE_TYPE, "Node"), PropertyInfo(Variant::STRING, "name"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	ADD_SIGNAL(MethodInfo("replicated_instance_added", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));
	ADD_SIGNAL(MethodInfo("replicated_instance_removed", PropertyInfo(Variant::INT, "scene_id"), PropertyInfo(Variant::OBJECT, "node", PROPERTY_HINT_RESOURCE_TYPE, "Node")));

	BIND_ENUM_CONSTANT(REPLICATION_MODE_NONE);
	BIND_ENUM_CONSTANT(REPLICATION_MODE_SERVER);
	BIND_ENUM_CONSTANT(REPLICATION_MODE_CUSTOM);
}
