#include "scene/main/node.h"

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

private:
	static void _spawn_despawn_cb(const NodePath &p_path, int p_from, const ResourceUID::ID &p_scene_id, const String &p_name, const PackedByteArray &p_data, bool p_spawn);
	void _spawn_despawn(int p_from, const ResourceUID::ID &p_scene_id, const String &p_name, const PackedByteArray &p_data, bool p_spawn);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Error spawn(Node *p_node, const PackedByteArray &p_data);
	MultiplayerSpawner();
};
