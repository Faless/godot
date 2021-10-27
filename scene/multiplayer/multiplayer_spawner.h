#include "scene/main/node.h"

#include "scene/resources/scene_replication_config.h"

class SceneReplicator : public Object {
	GDCLASS(SceneReplicator, Object);

protected:
	static SceneReplicator *singleton;

	SceneReplicator() { singleton = this; }

public:
	static SceneReplicator *get_singleton() { return singleton ? singleton : memnew(SceneReplicator); }
	void _spawn_receive(int p_from, ResourceUID::ID p_scene_id, const Variant &p_data, bool p_spawn, Ref<MultiplayerAPI> p_multiplayer);
	void _spawn_send(int p_peer, ResourceUID::ID p_scene_id, Object *p_obj, bool p_spawn);

public:
	void add_spawnable(Ref<MultiplayerAPI> p_multiplayer, const ResourceUID::ID &p_id, const TypedArray<StringName> &p_initial_state);
};

class MultiplayerSpawner;

class SpawnableNode : public RefCounted {
protected:
	Node *node = nullptr;
	MultiplayerSpawner *spawner = nullptr;

public:
	Node *get_node() { return node; }
	MultiplayerSpawner *get_spawner() { return spawner; }
	void reset() {
		node = nullptr;
		spawner = nullptr;
	}
	void setup(Node *p_node, MultiplayerSpawner *p_spawner) {
		node = p_node;
		spawner = p_spawner;
	}

	SpawnableNode(){};
};

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

private:
	Ref<SpawnableNode> spawning;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void add_spawnable(const ResourceUID::ID &p_id, const TypedArray<StringName> &p_initial_state);

	Error spawn(Node *p_node, const PackedByteArray &p_data);
	MultiplayerSpawner();
};
