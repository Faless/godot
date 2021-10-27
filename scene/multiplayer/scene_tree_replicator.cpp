#include "scene/multiplayer/scene_tree_replicator.h"

#include "core/multiplayer/multiplayer_api.h"

MultiplayerReplicationInterface *SceneTreeReplicatorInterface::_create() {
	return memnew(SceneTreeReplicatorInterface);
}

void SceneTreeReplicatorInterface::make_default() {
	MultiplayerAPI::create_default_replication_interface = _create;
}

SceneTreeReplicatorInterface::SceneTreeReplicatorInterface() {
}

SceneTreeReplicator::SceneTreeReplicator() {
}

SceneTreeReplicator::~SceneTreeReplicator() {
}
