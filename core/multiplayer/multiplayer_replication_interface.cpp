#include "core/multiplayer/multiplayer_replication_interface.h"

#include "core/multiplayer/multiplayer_api.h"

void MultiplayerReplicationInterface::set_multiplayer(MultiplayerAPI *p_multiplayer) {
	multiplayer = p_multiplayer;
	on_start();
}

Error MultiplayerReplicationInterface::send_raw(const uint8_t *p_buffer, int p_size, int p_peer, Multiplayer::TransferMode p_mode, int p_channel) {
	ERR_FAIL_COND_V(!p_buffer || p_size < 1, ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(!multiplayer, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!multiplayer->has_multiplayer_peer(), ERR_UNCONFIGURED);
	Ref<MultiplayerPeer> peer = multiplayer->get_multiplayer_peer();
	peer->set_target_peer(p_peer);
	peer->set_transfer_channel(p_channel);
	peer->set_transfer_mode(p_mode);
	return peer->put_packet(p_buffer, p_size);
}

Error MultiplayerReplicationInterface::_do_send(int p_peer, const PackedByteArray &p_data, Multiplayer::TransferMode p_mode, int p_channel, int p_cmd) {
	int size = p_data.size();
	if (packet_cache.size() < 1 + size) {
		packet_cache.resize(1 + size);
	}
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = p_cmd;
	if (size) {
		memcpy(&ptr[1], p_data.ptr(), size);
	}
	return send_raw(ptr, 1 + size, p_peer, p_mode, p_channel);
}

Error MultiplayerReplicationInterface::send_spawn(const PackedByteArray &p_data, int p_peer) {
	return _do_send(p_peer, p_data, Multiplayer::TRANSFER_MODE_RELIABLE, 0, (int)MultiplayerAPI::NETWORK_COMMAND_SPAWN);
}

Error MultiplayerReplicationInterface::send_despawn(const PackedByteArray &p_data, int p_peer) {
	return _do_send(p_peer, p_data, Multiplayer::TRANSFER_MODE_RELIABLE, 0, (int)MultiplayerAPI::NETWORK_COMMAND_DESPAWN);
}

Error MultiplayerReplicationInterface::send_sync(const PackedByteArray &p_data, int p_peer) {
	return _do_send(p_peer, p_data, Multiplayer::TRANSFER_MODE_UNRELIABLE, 0, (int)MultiplayerAPI::NETWORK_COMMAND_SYNC);
}

Error MultiplayerReplicationInterface::on_spawn(Object *p_obj, Variant p_config) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_spawn, p_obj, p_config, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_spawn is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_despawn(Object *p_obj, Variant p_config) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_despawn, p_obj, p_config, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_despawn is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_spawn_receive, p_from, p_buffer_len > 1 ? &p_buffer[1] : nullptr, p_buffer_len - 1, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_spawn_receive is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_despawn_receive, p_from, p_buffer_len > 1 ? &p_buffer[1] : nullptr, p_buffer_len - 1, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_despawn_receive is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_sync_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_sync_receive, p_from, p_buffer_len > 1 ? &p_buffer[1] : nullptr, p_buffer_len - 1, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_sync_receive is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_replication_start(Object *p_obj, Variant p_config) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_replication_start, p_obj, p_config, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_replication_start is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_replication_stop(Object *p_obj, Variant p_config) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_replication_stop, p_obj, p_config, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_replication_stop is unimplemented!");
	return ERR_UNCONFIGURED;
}
