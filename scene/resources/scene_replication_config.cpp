#include "scene_replication_config.h"

void SceneReplicationConfig::add_property(const NodePath &p_path, bool p_spawn, bool p_sync) {
	ERR_FAIL_COND(properties.find(p_path));
	properties.push_back(ReplicationProperty(p_path, p_spawn, p_sync));
}

void SceneReplicationConfig::remove_property(const NodePath &p_path) {
	properties.erase(p_path);
}

void SceneReplicationConfig::set_property_spawn(const NodePath &p_path, bool p_enabled) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND(!E);
	E->get().spawn = p_enabled;
}

void SceneReplicationConfig::set_property_sync(const NodePath &p_path, bool p_enabled) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND(!E);
	E->get().sync = p_enabled;
}

TypedArray<Array> SceneReplicationConfig::get_replication() const {
	Array out;
	for (const ReplicationProperty &prop : properties) {
		Array out_prop;
		out_prop.resize(3);
		out_prop[0] = prop.name;
		out_prop[1] = prop.spawn;
		out_prop[2] = prop.sync;
		out.push_back(out_prop);
	}
	return out;
}

void SceneReplicationConfig::set_replication(const TypedArray<Array> &p_replication) {
	properties.clear();
	for (int i = 0; i < p_replication.size(); i++) {
		Array arr = p_replication[i];
		ERR_CONTINUE(arr.size() != 3 || arr[0].get_type() != Variant::NODE_PATH || arr[1].get_type() != Variant::BOOL || arr[2].get_type() != Variant::BOOL);
		properties.push_back(ReplicationProperty(arr[0], arr[1], arr[2]));
	}
}

void SceneReplicationConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_property", "path", "spawn", "sync"), &SceneReplicationConfig::add_property);
	ClassDB::bind_method(D_METHOD("remove_property", "path"), &SceneReplicationConfig::remove_property);
	ClassDB::bind_method(D_METHOD("set_property_spawn", "path", "enabled"), &SceneReplicationConfig::set_property_spawn);
	ClassDB::bind_method(D_METHOD("set_property_sync", "path", "enabled"), &SceneReplicationConfig::set_property_sync);

	ClassDB::bind_method(D_METHOD("get_replication"), &SceneReplicationConfig::get_replication);
	ClassDB::bind_method(D_METHOD("set_replication", "replication"), &SceneReplicationConfig::set_replication);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "replication", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "set_replication", "get_replication");
}
