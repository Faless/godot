#include "core/multiplayer/multiplayer_replication_interface.h"

class SceneTreeReplicatorInterface : public MultiplayerReplicationInterface {
	GDCLASS(SceneTreeReplicatorInterface, MultiplayerReplicationInterface);

protected:
	static MultiplayerReplicationInterface *_create();

public:
	static void make_default();

	SceneTreeReplicatorInterface();
};

class SceneTreeReplicator {
public:
	SceneTreeReplicator();
	~SceneTreeReplicator();
};
