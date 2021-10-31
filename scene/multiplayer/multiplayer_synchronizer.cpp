#include "multiplayer_synchronizer.h"

void MultiplayerSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("sync"), &MultiplayerSynchronizer::sync);

	ClassDB::bind_method(D_METHOD("set_auto_sync_enabled", "enabled"), &MultiplayerSynchronizer::set_auto_sync_enabled);
	ClassDB::bind_method(D_METHOD("is_auto_sync_enabled"), &MultiplayerSynchronizer::is_auto_sync_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_sync"), "set_auto_sync_enabled", "is_auto_sync_enabled");

	ClassDB::bind_method(D_METHOD("set_root_path", "path"), &MultiplayerSynchronizer::set_root_path);
	ClassDB::bind_method(D_METHOD("get_root_path"), &MultiplayerSynchronizer::get_root_path);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "root_path"), "set_root_path", "get_root_path");

	ClassDB::bind_method(D_METHOD("set_replication_config", "config"), &MultiplayerSynchronizer::set_replication_config);
	ClassDB::bind_method(D_METHOD("get_replication_config"), &MultiplayerSynchronizer::get_replication_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "SceneReplicationConfig"), "set_replication_config", "get_replication_config");
}

void MultiplayerSynchronizer::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (!root_path.is_empty() && has_node(root_path)) {
			get_multiplayer()->spawn(this);
		}
	}
}

Error MultiplayerSynchronizer::sync() {
	return get_multiplayer()->sync(this, 0);
}

void MultiplayerSynchronizer::set_auto_sync_enabled(bool p_enabled) {
	auto_sync = p_enabled;
}

bool MultiplayerSynchronizer::is_auto_sync_enabled() const {
	return auto_sync;
}

void MultiplayerSynchronizer::set_replication_config(Ref<SceneReplicationConfig> p_config) {
	replication_config = p_config;
}

Ref<SceneReplicationConfig> MultiplayerSynchronizer::get_replication_config() {
	return replication_config;
}

void MultiplayerSynchronizer::set_root_path(const NodePath &p_path) {
	root_path = p_path;
}

NodePath MultiplayerSynchronizer::get_root_path() const {
	return root_path;
}
