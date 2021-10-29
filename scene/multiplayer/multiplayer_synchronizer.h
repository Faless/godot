#include "scene/main/node.h"

#include "scene/resources/scene_replication_config.h"

class MultiplayerSynchronizer : public Node {
	GDCLASS(MultiplayerSynchronizer, Node);

private:
	bool auto_sync = true;
	Ref<SceneReplicationConfig> replication_config;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Error sync();

	void set_auto_sync_enabled(bool p_enabled);
	bool is_auto_sync_enabled() const;

	void set_replication_config(Ref<SceneReplicationConfig> p_config);
	Ref<SceneReplicationConfig> get_replication_config();

	MultiplayerSynchronizer() {}
};
