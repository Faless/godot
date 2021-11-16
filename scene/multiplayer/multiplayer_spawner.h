#ifndef MULTIPLAYER_SPAWNER_H
#define MULTIPLAYER_SPAWNER_H

#include "scene/main/node.h"

#include "core/variant/typed_array.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/scene_replication_config.h"

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

private:
	TypedArray<PackedScene> spawnable_scenes;
	Set<ResourceUID::ID> spawnable_ids;
	NodePath spawn_path;

	Set<ObjectID> tracked_nodes;
	Set<ObjectID> remote_nodes;
	bool auto_spawn = false;

	void _connect_node(Node *p_node);
	void _node_exit(ObjectID p_id);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _node_added(Node *p_node);

public:
	bool can_spawn(const ResourceUID::ID &p_id) const { return spawnable_ids.has(p_id); }
	TypedArray<PackedScene> get_spawnable_scenes();
	void set_spawnable_scenes(TypedArray<PackedScene> p_scenes);
	NodePath get_spawn_path() const;
	void set_spawn_path(const NodePath &p_path);
	bool is_auto_spawning() const;
	void set_auto_spawning(bool p_enabled);

	Node *get_currently_spawning();

	Error spawn(Node *p_node, int p_peer);
	void track(Node *p_node);

	Error remote_spawn(Node *p_node, const String &p_name);
	Error remote_despawn(Node *p_node);

	MultiplayerSpawner() {}
};

#endif // MULTIPLAYER_SPAWNER_H
