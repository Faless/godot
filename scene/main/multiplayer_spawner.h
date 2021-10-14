#include "scene/main/node.h"

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

private:
	void _spawn_receive(int p_from, ResourceUID::ID p_scene_id, const Variant &p_data, bool p_spawn);
	void _spawn_send(int p_peer, ResourceUID::ID p_scene_id, Object *p_obj, bool p_spawn);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void add_spawnable(const ResourceUID::ID &p_id, const TypedArray<StringName> &p_initial_state);

	Error spawn(Node *p_node, const PackedByteArray &p_data);
	MultiplayerSpawner();
};
