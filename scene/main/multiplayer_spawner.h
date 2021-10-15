#include "scene/main/node.h"

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

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void add_spawnable(const ResourceUID::ID &p_id, const TypedArray<StringName> &p_initial_state);

	Error spawn(Node *p_node, const PackedByteArray &p_data);
	MultiplayerSpawner();
};
