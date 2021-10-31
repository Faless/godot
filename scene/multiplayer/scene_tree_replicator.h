#ifndef SCENE_TREE_REPLICATOR_INTERFACE_H
#define SCENE_TREE_REPLICATOR_INTERFACE_H

#include "core/multiplayer/multiplayer_replication_interface.h"

#include "scene/multiplayer/multiplayer_spawner.h"

class SceneTreeReplicatorInterface : public MultiplayerReplicationInterface {
	GDCLASS(SceneTreeReplicatorInterface, MultiplayerReplicationInterface);

private:
	struct TrackedObject {
		ObjectID id;
		ObjectID spawner;
		ObjectID synchronizer;
		bool pending = true;

		bool operator==(const ObjectID &p_other) {
			return id == p_other;
		}

		TrackedObject() {}

		TrackedObject(const Node *p_node, const Node *p_spawner) {
			id = p_node->get_instance_id();
			spawner = p_node->get_instance_id();
		}

		TrackedObject(const Node *p_node, const Node *p_spawner, const Node *p_synchronizer) :
				TrackedObject(p_node, p_spawner) {
			synchronizer = p_synchronizer->get_instance_id();
		}
	};

	HashMap<ObjectID, TrackedObject> tracked_objects;

	Error _send_spawn_despawn(MultiplayerSpawner *spawner, Node *p_node, int p_peer, bool p_spawn);
	Error _spawn_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len, bool p_spawn);

protected:
	static MultiplayerReplicationInterface *_create();

public:
	static void make_default();

	virtual Error on_spawn_send(Object *p_obj, int p_peer) override;
	virtual Error on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) override;
	virtual Error on_despawn_send(Object *p_obj, int p_peer) override;
	virtual Error on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) override;
	virtual Error on_sync_send(Object *p_obj, int p_peer) override;
	virtual Error on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) override;

	SceneTreeReplicatorInterface();
};

class SceneTreeReplicator {
public:
	SceneTreeReplicator();
	~SceneTreeReplicator();
};
#endif // SCENE_TREE_REPLICATOR_INTERFACE_H
