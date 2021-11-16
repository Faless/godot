#ifndef SCENE_REPLICATION_CONFIG_H
#define SCENE_REPLICATION_CONFIG_H

#include "core/io/resource.h"

#include "core/variant/typed_array.h"

class SceneReplicationConfig : public Resource {
	GDCLASS(SceneReplicationConfig, Resource);
	OBJ_SAVE_TYPE(SceneReplicationConfig);
	RES_BASE_EXTENSION("repl");

private:
	struct ReplicationProperty {
		NodePath name;
		bool spawn = false;
		bool sync = false;

		bool operator==(const ReplicationProperty &p_to) {
			return name == p_to.name;
		}

		ReplicationProperty() {}

		ReplicationProperty(const NodePath &p_name) {
			name = p_name;
		}

		ReplicationProperty(const NodePath &p_name, bool p_spawn, bool p_sync) {
			name = p_name;
			spawn = p_spawn;
			sync = p_sync;
		}
	};

	List<ReplicationProperty> properties;

protected:
	static void _bind_methods();

	static Object *_get_prop_target(Object *p_obj, const NodePath &p_prop);
	static Error _get_state(const List<NodePath> &p_properties, Object *p_obj, Vector<Variant> &r_variant);
	static Error _decode_state(const List<NodePath> &p_cfg, Object *p_obj, const uint8_t *p_buffer, int p_len, int &r_len, bool p_raw = false);

public:
	void add_property(const NodePath &p_path, bool p_spawn, bool p_sync);
	void remove_property(const NodePath &p_path);

	void set_property_spawn(const NodePath &p_path, bool p_enabled);
	void set_property_sync(const NodePath &p_path, bool p_enabled);

	TypedArray<Array> get_replication() const;
	void set_replication(const TypedArray<Array> &p_replication);

	SceneReplicationConfig() {}

	PackedByteArray encode_spawn_state(Object *p_obj);
	Error decode_spawn_state(Object *p_obj, const PackedByteArray &p_state);
};

#endif // SCENE_REPLICATION_CONFIG_H
