#include "core/multiplayer/multiplayer_replication_interface.h"

#include "core/multiplayer/multiplayer_api.h"

void MultiplayerReplicationInterface::set_multiplayer(MultiplayerAPI *p_multiplayer) {
	multiplayer = p_multiplayer;
}

Error MultiplayerReplicationInterface::_do_send(int p_peer, const PackedByteArray &p_data, Multiplayer::TransferMode p_mode, int p_channel, int p_cmd) {
	ERR_FAIL_COND_V(!multiplayer, ERR_UNCONFIGURED);
	int size = p_data.size();
	if (packet_cache.size() < 1 + size) {
		packet_cache.resize(1 + size);
	}
	uint8_t *ptr = packet_cache.ptrw();
	ptr[0] = p_cmd;
	if (size) {
		memcpy(&ptr[1], p_data.ptr(), size);
	}
	Ref<MultiplayerPeer> peer = multiplayer->get_multiplayer_peer();
	peer->set_target_peer(p_peer);
	peer->set_transfer_channel(p_channel);
	peer->set_transfer_mode(p_mode);
	return peer->put_packet(ptr, 1 + size);
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

Error MultiplayerReplicationInterface::on_spawn_send(Object *p_obj, int p_peer) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_spawn_send, p_obj, p_peer, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_spawn_send is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_spawn_receive, p_from, p_buffer, p_buffer_len, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_spawn_receive is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_despawn_send(Object *p_obj, int p_peer) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_despawn_send, p_obj, p_peer, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_despawn_send is unimplemented!");
	return ERR_UNCONFIGURED;
}

Error MultiplayerReplicationInterface::on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len) {
	int ret = 0;
	if (GDVIRTUAL_CALL(_on_despawn_receive, p_from, p_buffer, p_buffer_len, ret)) {
		return (Error)ret;
	}
	WARN_PRINT_ONCE("MultiplayerReplicationInterface::_on_spawn_receive is unimplemented!");
	return ERR_UNCONFIGURED;
}
