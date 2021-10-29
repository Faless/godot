#include "multiplayer_synchronizer.h"

void MultiplayerSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("sync"), &MultiplayerSynchronizer::sync);

	ClassDB::bind_method(D_METHOD("set_auto_sync_enabled", "enabled"), &MultiplayerSynchronizer::set_auto_sync_enabled);
	ClassDB::bind_method(D_METHOD("is_auto_sync_enabled"), &MultiplayerSynchronizer::is_auto_sync_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_sync"), "set_auto_sync_enabled", "is_auto_sync_enabled");

	ClassDB::bind_method(D_METHOD("set_replication_config", "config"), &MultiplayerSynchronizer::set_replication_config);
	ClassDB::bind_method(D_METHOD("get_replication_config"), &MultiplayerSynchronizer::get_replication_config);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE, "SceneReplicationConfig"), "set_replication_config", "get_replication_config");
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

void MultiplayerSynchronizer::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		// TODO will need to advertize this to the API, so we know how to spawn.
	}
}

void MultiplayerSynchronizer::set_replication_config(Ref<SceneReplicationConfig> p_config) {
	replication_config = p_config;
}

Ref<SceneReplicationConfig> MultiplayerSynchronizer::get_replication_config() {
	return replication_config;
}
