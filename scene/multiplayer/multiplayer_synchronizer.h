#ifndef MULTIPLAYER_SYNCHRONIZER_H
#define MULTIPLAYER_SYNCHRONIZER_H

#include "scene/main/node.h"

#include "scene/resources/scene_replication_config.h"

class MultiplayerSynchronizer : public Node {
	GDCLASS(MultiplayerSynchronizer, Node);

private:
	bool auto_sync = true;
	Ref<SceneReplicationConfig> replication_config;
	NodePath root_path;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Error sync();

	void set_auto_sync_enabled(bool p_enabled);
	bool is_auto_sync_enabled() const;

	void set_replication_config(Ref<SceneReplicationConfig> p_config);
	Ref<SceneReplicationConfig> get_replication_config();

	void set_root_path(const NodePath &p_path);
	NodePath get_root_path() const;

	MultiplayerSynchronizer() {}
};

#endif // MULTIPLAYER_SYNCHRONIZER_H
