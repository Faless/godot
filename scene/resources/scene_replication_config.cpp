#include "scene_replication_config.h"

#include "core/multiplayer/multiplayer_api.h"
#include "scene/main/node.h"

Object *SceneReplicationConfig::_get_prop_target(Object *p_obj, const NodePath &p_path) {
	if (p_path.get_name_count() == 0) {
		return p_obj;
	}
	Node *node = Object::cast_to<Node>(p_obj);
	ERR_FAIL_COND_V_MSG(!node || !node->has_node(p_path), nullptr, vformat("Node '%s' not found.", p_path));
	return node->get_node(p_path);
}

Error SceneReplicationConfig::_get_state(const List<NodePath> &p_properties, Object *p_obj, List<Variant> &r_variant) {
	ERR_FAIL_COND_V_MSG(!p_obj, ERR_INVALID_PARAMETER, "Cannot encode null object");
	for (const NodePath &prop : p_properties) {
		bool valid = false;
		const Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_COND_V(!obj, FAILED);
		const Variant v = obj->get(prop.get_concatenated_subnames(), &valid);
		ERR_FAIL_COND_V_MSG(!valid, ERR_INVALID_DATA, vformat("Property '%s' not found.", prop));
		r_variant.push_back(v);
	}
	return OK;
}

Error SceneReplicationConfig::_encode_state(const List<Variant> &p_variants, uint8_t *p_buffer, int &r_len, bool *r_raw) {
	r_len = 0;
	int size = 0;

	// Try raw encoding optimization.
	if (r_raw && p_variants.size() == 1) {
		*r_raw = false;
		const Variant v = p_variants[0];
		if (v.get_type() == Variant::PACKED_BYTE_ARRAY) {
			*r_raw = true;
			const PackedByteArray pba = v;
			if (p_buffer) {
				memcpy(p_buffer, pba.ptr(), pba.size());
			}
			r_len += pba.size();
		} else {
			MultiplayerAPI::encode_and_compress_variant(v, p_buffer, size, false);
			r_len += size;
		}
		return OK;
	}

	// Regular encoding.
	for (const Variant &v : p_variants) {
		MultiplayerAPI::encode_and_compress_variant(v, p_buffer ? p_buffer + r_len : nullptr, size, false);
		r_len += size;
	}
	return OK;
}

Error SceneReplicationConfig::_decode_state(const List<NodePath> &p_properties, Object *p_obj, const uint8_t *p_buffer, int p_len, int &r_len, bool p_raw) {
	r_len = 0;
	int argc = p_properties.size();
	if (argc == 0 && p_raw) {
		ERR_FAIL_COND_V_MSG(p_len != 0, ERR_INVALID_DATA, "Buffer has trailing bytes.");
		return OK;
	}
	ERR_FAIL_COND_V(p_raw && argc != 1, ERR_INVALID_DATA);
	if (p_raw) {
		r_len = p_len;
		PackedByteArray pba;
		pba.resize(p_len);
		memcpy(pba.ptrw(), p_buffer, p_len);
		const NodePath &prop = p_properties[0];
		Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_COND_V(!obj, FAILED);
		obj->set(prop.get_concatenated_subnames(), pba);
		return OK;
	}

	Vector<Variant> args;
	Vector<const Variant *> argp;
	args.resize(argc);

	for (int i = 0; i < argc; i++) {
		ERR_FAIL_COND_V_MSG(r_len >= p_len, ERR_INVALID_DATA, "Invalid packet received. Size too small.");

		int vlen;
		Error err = MultiplayerAPI::decode_and_decompress_variant(args.write[i], &p_buffer[r_len], p_len - r_len, &vlen, false);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Invalid packet received. Unable to decode state variable.");
		r_len += vlen;
	}
	ERR_FAIL_COND_V_MSG(p_len - r_len != 0, ERR_INVALID_DATA, "Buffer has trailing bytes.");

	int i = 0;
	for (const NodePath &prop : p_properties) {
		Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_COND_V(!obj, FAILED);
		obj->set(prop.get_concatenated_subnames(), args[i]);
		i += 1;
	}
	return OK;
}

PackedByteArray SceneReplicationConfig::encode_spawn_state(Object *p_obj) {
	// TODO can do much better.
	PackedByteArray out;
	List<Variant> vars;
	List<NodePath> props;
	int len = 0;
	for (const ReplicationProperty &prop : properties) {
		if (prop.spawn) {
			props.push_back(prop.name);
		}
	}
	Error err = _get_state(props, p_obj, vars);
	ERR_FAIL_COND_V_MSG(err != OK, out, "Unable to retrieve object state.");
	err = _encode_state(vars, nullptr, len);
	ERR_FAIL_COND_V_MSG(err != OK, out, "Unable to encode object state.");
	out.resize(len);
	_encode_state(vars, out.ptrw(), len);
	return out;
}

Error SceneReplicationConfig::decode_spawn_state(Object *p_obj, const PackedByteArray &p_state) {
	// TODO can do much better.
	PackedByteArray out;
	List<NodePath> props;
	for (const ReplicationProperty &prop : properties) {
		if (prop.spawn) {
			props.push_back(prop.name);
		}
	}
	int size;
	return _decode_state(props, p_obj, p_state.ptr(), p_state.size(), size);
}

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
