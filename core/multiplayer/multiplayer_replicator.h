/*************************************************************************/
/*  multiplayer_replicator.h                                             */
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

#ifndef MULTIPLAYER_REPLICATOR_H
#define MULTIPLAYER_REPLICATOR_H

#include "core/multiplayer/multiplayer_api.h"

#include "core/io/resource_uid.h"
#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"

class MultiplayerReplicator : public Object {
	GDCLASS(MultiplayerReplicator, Object);

public:
	using SpawnDespawnFunc = void (*)(const NodePath &p_path, int p_from, const ResourceUID::ID &p_scene_id, const String &p_name, const PackedByteArray &p_data, bool p_spawn);
	using NetID = uint16_t;

	enum {
		SPAWN_CMD_OFFSET = 9,
		SYNC_CMD_OFFSET = 9,
	};

	enum ReplicationMode {
		REPLICATION_MODE_NONE,
		REPLICATION_MODE_SERVER,
		REPLICATION_MODE_CUSTOM,
	};

	struct SceneConfig {
		ReplicationMode mode;
		uint64_t sync_interval = 0;
		uint64_t sync_last = 0;
		uint8_t sync_recv = 0;
		NetID last_id = 0;
		List<StringName> properties;
		List<StringName> sync_properties;
		Callable on_spawn_despawn_send;
		Callable on_spawn_despawn_receive;
		Callable on_sync_send;
		Callable on_sync_receive;
	};

protected:
	static void _bind_methods();

private:
	template <class K1, class K2>
	struct BiMap {
		Map<K1, K2> map1;
		Map<K2, K1> map2;

		void insert(K1 p_k1, K2 p_k2) {
			ERR_FAIL_COND(map1.has(p_k1) || map2.has(p_k2));
			map1[p_k1] = p_k2;
			map2[p_k2] = p_k1;
		}
		void erase(K1 p_k1) {
			ERR_FAIL_COND(!map1.has(p_k1));
			const K2 &k2 = map1[p_k1];
			ERR_FAIL_COND(!map2.has(k2));
			map2.erase(k2);
			map1.erase(p_k1);
		}
		void erase(K2 p_k2) {
			ERR_FAIL_COND(!map2.has(p_k2));
			const K1 &k1 = map2[p_k2];
			ERR_FAIL_COND(!map1.has(k1));
			map1.erase(k1);
			map2.erase(p_k2);
		}
		bool has(K1 p_k1) {
			return map1.has(p_k1);
		}
		bool has(K2 p_k2) {
			return map2.has(p_k2);
		}
		int size() {
			return map1.size();
		}
		const Map<K1, K2> &get_map() const {
			return map1;
		}
		const Map<K1, K2> &get_reverse_map() const {
			return map2;
		}
	};

	enum {
		BYTE_OR_ZERO_SHIFT = MultiplayerAPI::CMD_FLAG_0_SHIFT,
	};

	enum {
		BYTE_OR_ZERO_FLAG = 1 << BYTE_OR_ZERO_SHIFT,
	};

	MultiplayerAPI *multiplayer = nullptr;
	Vector<uint8_t> packet_cache;
	HashMap<NodePath, SpawnDespawnFunc> spawners;
	Map<ResourceUID::ID, SceneConfig> replications;
	Map<ObjectID, ResourceUID::ID> replicated_nodes;
	HashMap<ResourceUID::ID, BiMap<ObjectID, NetID>> tracked_objects;

	// Encoding
	Error _get_state(const List<StringName> &p_properties, const Object *p_obj, List<Variant> &r_variant);
	Error _encode_state(const List<Variant> &p_variants, uint8_t *p_buffer, int &r_len, bool *r_raw = nullptr);
	Error _decode_state(const List<StringName> &p_cfg, Object *p_obj, const uint8_t *p_buffer, int p_len, int &r_len, bool p_raw = false);

	// Spawn
	Error _spawn_despawn(ResourceUID::ID p_scene_id, Object *p_obj, int p_peer, bool p_spawn);
	Error _send_spawn_despawn(int p_peer_id, const ResourceUID::ID &p_scene_id, const Variant &p_data, bool p_spawn);
	void _process_default_spawn_despawn(int p_from, const ResourceUID::ID &p_scene_id, const uint8_t *p_packet, int p_packet_len, bool p_spawn);
	Error _send_default_spawn_despawn(int p_peer_id, const ResourceUID::ID &p_scene_id, Object *p_obj, const NodePath &p_path, bool p_spawn);

	// Sync
	void _process_default_sync(const ResourceUID::ID &p_id, const uint8_t *p_packet, int p_packet_len);
	Error _sync_all_default(const ResourceUID::ID &p_scene_id, int p_peer);
	void _track(const ResourceUID::ID &p_scene_id, Object *p_object);
	void _untrack(const ResourceUID::ID &p_scene_id, Object *p_object);

public:
	void clear();

	// Encoding
	PackedByteArray encode_state(const ResourceUID::ID &p_scene_id, const Object *p_node, bool p_initial);
	Error decode_state(const ResourceUID::ID &p_scene_id, Object *p_node, PackedByteArray p_data, bool p_initial);

	// Spawn
	void register_spawner(const NodePath &p_path, SpawnDespawnFunc p_func) { spawners[p_path] = p_func; }
	void deregister_spawner(const NodePath &p_path) { spawners.erase(p_path); }
	Error spawn_config(const ResourceUID::ID &p_id, ReplicationMode p_mode, const TypedArray<StringName> &p_props = TypedArray<StringName>(), const Callable &p_on_send = Callable(), const Callable &p_on_recv = Callable());
	Error spawn(ResourceUID::ID p_scene_id, Object *p_obj, int p_peer = 0);
	Error despawn(ResourceUID::ID p_scene_id, Object *p_obj, int p_peer = 0);
	Error send_despawn(int p_peer_id, const ResourceUID::ID &p_scene_id, const Variant &p_data = Variant(), const NodePath &p_path = NodePath());
	Error send_spawn(int p_peer_id, const ResourceUID::ID &p_scene_id, const Variant &p_data = Variant(), const NodePath &p_path = NodePath());

	// Sync
	Error sync_config(const ResourceUID::ID &p_id, uint64_t p_interval, const TypedArray<StringName> &p_props = TypedArray<StringName>(), const Callable &p_on_send = Callable(), const Callable &p_on_recv = Callable());
	Error sync_all(const ResourceUID::ID &p_scene_id, int p_peer);
	Error send_sync(int p_peer_id, const ResourceUID::ID &p_scene_id, PackedByteArray p_data, Multiplayer::TransferMode p_mode, int p_channel);
	void track(const ResourceUID::ID &p_scene_id, Object *p_object);
	void untrack(const ResourceUID::ID &p_scene_id, Object *p_object);

	// Used by MultiplayerAPI
	void spawn_all(int p_peer);
	void process_spawn_despawn(int p_from, const uint8_t *p_packet, int p_packet_len, bool p_spawn);
	void process_sync(int p_from, const uint8_t *p_packet, int p_packet_len);
	void scene_enter_exit_notify(const String &p_scene, Node *p_node, bool p_enter);
	void poll();

	MultiplayerReplicator(MultiplayerAPI *p_multiplayer) {
		multiplayer = p_multiplayer;
	}
};

VARIANT_ENUM_CAST(MultiplayerReplicator::ReplicationMode);

#endif // MULTIPLAYER_REPLICATOR_H
