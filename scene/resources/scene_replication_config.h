/*************************************************************************/
/*  scene_replication_config.h                                           */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

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
	List<NodePath> spawn_props;
	List<NodePath> sync_props;

protected:
	static void _bind_methods();

	static Object *_get_prop_target(Object *p_obj, const NodePath &p_prop);

public:
	static Error get_state(const List<NodePath> &p_properties, Object *p_obj, Vector<Variant> &r_variant, Vector<const Variant *> &r_variant_ptrs);
	static Error set_state(const List<NodePath> &p_properties, Object *p_obj, const Vector<Variant> &p_state);

	void add_property(const NodePath &p_path, bool p_spawn, bool p_sync);
	void remove_property(const NodePath &p_path);

	void set_property_spawn(const NodePath &p_path, bool p_enabled);
	void set_property_sync(const NodePath &p_path, bool p_enabled);

	TypedArray<Array> get_replication() const;
	void set_replication(const TypedArray<Array> &p_replication);

	const List<NodePath> &get_spawn_properties() { return spawn_props; }
	const List<NodePath> &get_sync_properties() { return sync_props; }

	SceneReplicationConfig() {}
};

#endif // SCENE_REPLICATION_CONFIG_H
