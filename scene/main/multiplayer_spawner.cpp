#include "multiplayer_spawner.h"

#include "core/multiplayer/multiplayer_replicator.h"

void MultiplayerSpawner::_bind_methods() {
}

void MultiplayerSpawner::_notification(int p_what) {
	// TODO register/deregister spawner.
	if (p_what == NOTIFICATION_ENTER_TREE) {
		//get_multiplayer()->get_replicator()->register_spawner(get_path());
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		//get_multiplayer()->get_replicator()->deregister_spawner(get_path());
	}
}

Error MultiplayerSpawner::spawn(Node *p_node, const PackedByteArray &p_data) {
	ERR_FAIL_COND_V(!is_inside_tree(), ERR_UNCONFIGURED);
	// TODO must be a scene, must be replicated!
	const String scene = p_node->get_scene_file_path();
	if (scene.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	ResourceUID::ID id = ResourceLoader::get_resource_uid(scene);
	get_parent()->add_child(p_node);
	get_multiplayer()->get_replicator()->spawn(id, p_node, 0);
	return OK;
}

MultiplayerSpawner::MultiplayerSpawner() {
}
