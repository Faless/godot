#ifndef SCENE_TREE_REPLICATOR_INTERFACE_H
#define SCENE_TREE_REPLICATOR_INTERFACE_H

#include "core/multiplayer/multiplayer_replication_interface.h"

#include "core/multiplayer/multiplayer_api.h"
#include "scene/multiplayer/multiplayer_spawner.h"
#include "scene/multiplayer/multiplayer_synchronizer.h"

class SceneTreeReplicatorInterface : public MultiplayerReplicationInterface {
	GDCLASS(SceneTreeReplicatorInterface, MultiplayerReplicationInterface);

private:
	class NetID {
		uint32_t peer_id = 0;
		uint32_t net_id = 0;

	public:
		uint32_t get_id() const { return net_id; }
		uint32_t get_peer() const { return peer_id; }
		bool operator==(const NetID &p_other) const { return net_id == p_other.net_id && peer_id == p_other.peer_id; }
		operator uint64_t() const { return (uint64_t)peer_id << 32 || net_id; }
		bool is_valid() const { return net_id != 0; }
		bool is_null() const { return net_id == 0; }
		NetID() {}
		NetID(uint32_t p_net_id) { net_id = p_net_id; }
		NetID(uint32_t p_net_id, uint32_t p_peer_id) {
			net_id = p_net_id;
			peer_id = p_peer_id;
		}
	};

	struct TrackedObject {
		ObjectID id;
		NetID net_id;
		ObjectID spawner;
		ObjectID synchronizer;
		Variant args;
		bool spawn_pending = false;

		bool operator==(const ObjectID &p_other) { return id == p_other; }

		Node *get_node() const { return id.is_valid() ? Object::cast_to<Node>(ObjectDB::get_instance(id)) : nullptr; }
		MultiplayerSpawner *get_spawner() const { return spawner.is_valid() ? Object::cast_to<MultiplayerSpawner>(ObjectDB::get_instance(spawner)) : nullptr; }
		MultiplayerSynchronizer *get_synchronizer() const { return synchronizer.is_valid() ? Object::cast_to<MultiplayerSynchronizer>(ObjectDB::get_instance(synchronizer)) : nullptr; }
		bool is_custom() const { return args.get_type() == Variant::ARRAY; }

		TrackedObject() {}
		TrackedObject(const ObjectID &p_id) { id = p_id; }
		TrackedObject(const ObjectID &p_id, const NetID &p_net_id) {
			id = p_id;
			net_id = p_net_id;
		}
	};

	uint32_t last_net_id = 0;
	HashMap<NetID, TrackedObject> remote_objects;
	HashMap<ObjectID, TrackedObject> tracked_objects;
	ObjectID spawning;
	PackedByteArray *spawning_state = nullptr;

	Error _send_spawn(const TrackedObject &p_tracked, int p_peer);
	Error _send_despawn(const TrackedObject &p_tracked, int p_peer);

	bool is_spawning(Object *p_obj) { return p_obj && spawning == p_obj->get_instance_id(); }
	bool has_authority(const TrackedObject &p_tracked) const;

protected:
	static MultiplayerReplicationInterface *_create();

public:
	static void make_default();

	virtual Error on_replication_start(Object *p_obj, Variant p_config) override;
	virtual Error on_replication_stop(Object *p_obj, Variant p_config) override;
	virtual void on_network_process() override;

	virtual Error on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) override;
	virtual Error on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) override;
	virtual Error on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) override;

	SceneTreeReplicatorInterface();
};

#endif // SCENE_TREE_REPLICATOR_INTERFACE_H
