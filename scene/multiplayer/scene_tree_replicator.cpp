#include "scene_tree_replicator.h"

#include "core/io/marshalls.h"
#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/node.h"
#include "scene/multiplayer/multiplayer_spawner.h"

MultiplayerReplicationInterface *SceneTreeReplicatorInterface::_create() {
	return memnew(SceneTreeReplicatorInterface);
}

Error SceneTreeReplicatorInterface::_send_spawn(MultiplayerSpawner *p_spawner, Node *p_node, int p_peer) {
	ERR_FAIL_COND_V(!p_spawner, ERR_BUG);
	ERR_FAIL_COND_V(!multiplayer, ERR_BUG);
	ERR_FAIL_COND_V(!p_node, ERR_BUG);
	PackedByteArray packet;
	int ofs = 0;

	// Prepare simplified path
	//PackedByteArray state = multiplayer->get_replicator()->encode_state(p_scene_id, node, true);
	PackedByteArray state; // FIXME

	const Node *root_node = multiplayer->get_root_node();
	ERR_FAIL_COND_V(!root_node, ERR_UNCONFIGURED);
	NodePath rel_path = (root_node->get_path()).rel_path_to(p_spawner->get_path());

	int path_id = 0;
	multiplayer->send_confirm_path(p_spawner, rel_path, p_peer, path_id);

	// Encode name and parent ID.
	CharString cname = p_node->get_name().operator String().utf8();
	int nlen = encode_cstring(cname.get_data(), nullptr);
	packet.resize(4 + 4 + nlen + state.size());
	uint8_t *ptr = packet.ptrw();
	ofs = 0;
	ofs += encode_uint32(path_id, &ptr[ofs]);
	ofs += encode_uint32(nlen, &ptr[ofs]);
	ofs += encode_cstring(cname.get_data(), &ptr[ofs]);
	if (state.size()) {
		memcpy(&ptr[ofs], state.ptr(), state.size());
	}
	return send_spawn(packet, p_peer);
}

void SceneTreeReplicatorInterface::make_default() {
	MultiplayerAPI::create_default_replication_interface = _create;
}

Error SceneTreeReplicatorInterface::on_spawn_send(Object *p_obj, int p_peer) {
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V(!node, ERR_INVALID_PARAMETER);
	MultiplayerSpawner *spawner = Object::cast_to<MultiplayerSpawner>(p_obj);
	ERR_FAIL_COND_V(!spawner, ERR_INVALID_PARAMETER);
	_send_spawn(spawner, spawner->get_currently_spawning(), p_peer);
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
