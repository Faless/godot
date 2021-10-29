#include "multiplayer_synchronizer.h"

void MultiplayerSynchronizer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("sync"), &MultiplayerSynchronizer::sync);
	ClassDB::bind_method(D_METHOD("set_auto_sync_enabled", "enabled"), &MultiplayerSynchronizer::set_auto_sync_enabled);
	ClassDB::bind_method(D_METHOD("is_auto_sync_enabled"), &MultiplayerSynchronizer::is_auto_sync_enabled);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_sync"), "set_auto_sync_enabled", "is_auto_sync_enabled");
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
