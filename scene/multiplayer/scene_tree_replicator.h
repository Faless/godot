#include "core/multiplayer/multiplayer_replication_interface.h"

class SceneTreeReplicatorInterface : public MultiplayerReplicationInterface {
	GDCLASS(SceneTreeReplicatorInterface, MultiplayerReplicationInterface);

private:
	Map<int, ObjectID> tracked_objects;

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
