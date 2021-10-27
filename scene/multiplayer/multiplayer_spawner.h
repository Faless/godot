#include "scene/main/node.h"

#include "scene/resources/scene_replication_config.h"

#if 0
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
#endif

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

private:
	Node *spawning = nullptr;
	List<Ref<PackedScene>> spawnable_scenes;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	TypedArray<PackedScene> get_spawnable_scenes() const;
	void set_spawnable_scenes(const TypedArray<PackedScene> &p_scenes);

	Node *get_currently_spawning();

	Error spawn(Node *p_node, int p_peer);
	MultiplayerSpawner() {}
};
