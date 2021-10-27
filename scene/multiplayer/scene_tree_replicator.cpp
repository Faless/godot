#include "scene/multiplayer/scene_tree_replicator.h"

#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/node.h"

MultiplayerReplicationInterface *SceneTreeReplicatorInterface::_create() {
	return memnew(SceneTreeReplicatorInterface);
}

void SceneTreeReplicatorInterface::make_default() {
	MultiplayerAPI::create_default_replication_interface = _create;
}

Error SceneTreeReplicatorInterface::on_spawn_send(Object *p_obj, int p_peer) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	return OK;
}

Error SceneTreeReplicatorInterface::on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	return OK;
}

Error SceneTreeReplicatorInterface::on_despawn_send(Object *p_obj, int p_peer) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	return OK;
}

Error SceneTreeReplicatorInterface::on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	return OK;
}

Error SceneTreeReplicatorInterface::on_sync_send(Object *p_obj, int p_peer) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	return OK;
}

Error SceneTreeReplicatorInterface::on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	return OK;
}

SceneTreeReplicatorInterface::SceneTreeReplicatorInterface() {
}

SceneTreeReplicator::SceneTreeReplicator() {
}

SceneTreeReplicator::~SceneTreeReplicator() {
}
