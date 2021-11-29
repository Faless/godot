/*************************************************************************/
/*  scene_replication_config.cpp                                         */
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

Error SceneReplicationConfig::get_state(const List<NodePath> &p_properties, Object *p_obj, Vector<Variant> &r_variant, Vector<const Variant *> &r_variant_ptrs) {
	ERR_FAIL_COND_V(!p_obj, ERR_INVALID_PARAMETER);
	r_variant.resize(p_properties.size());
	r_variant_ptrs.resize(r_variant.size());
	int i = 0;
	for (const NodePath &prop : p_properties) {
		bool valid = false;
		const Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_COND_V(!obj, FAILED);
		r_variant.write[i] = obj->get(prop.get_concatenated_subnames(), &valid);
		r_variant_ptrs.write[i] = &r_variant[i];
		ERR_FAIL_COND_V_MSG(!valid, ERR_INVALID_DATA, vformat("Property '%s' not found.", prop));
		i++;
	}
	return OK;
}

Error SceneReplicationConfig::set_state(const List<NodePath> &p_properties, Object *p_obj, const Vector<Variant> &p_state) {
	ERR_FAIL_COND_V(!p_obj, ERR_INVALID_PARAMETER);
	int i = 0;
	for (const NodePath &prop : p_properties) {
		Object *obj = _get_prop_target(p_obj, prop);
		ERR_FAIL_COND_V(!obj, FAILED);
		obj->set(prop.get_concatenated_subnames(), p_state[i]);
		i += 1;
	}
	return OK;
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
	spawn_props.clear();
	for (const ReplicationProperty &prop : properties) {
		if (prop.spawn) {
			spawn_props.push_back(p_path);
		}
	}
}

void SceneReplicationConfig::set_property_sync(const NodePath &p_path, bool p_enabled) {
	List<ReplicationProperty>::Element *E = properties.find(p_path);
	ERR_FAIL_COND(!E);
	E->get().sync = p_enabled;
	sync_props.clear();
	for (const ReplicationProperty &prop : properties) {
		if (prop.sync) {
			NodePath np = p_path;
			sync_props.push_back(p_path);
		}
	}
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
		ReplicationProperty prop(arr[0], arr[1], arr[2]);
		properties.push_back(prop);
		if (prop.sync) {
			sync_props.push_back(prop.name);
		}
		if (prop.spawn) {
			sync_props.push_back(prop.name);
		}
	}
}

void SceneReplicationConfig::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_property", "path", "spawn", "sync"), &SceneReplicationConfig::add_property);
	ClassDB::bind_method(D_METHOD("remove_property", "path"), &SceneReplicationConfig::remove_property);
	ClassDB::bind_method(D_METHOD("set_property_spawn", "path", "enabled"), &SceneReplicationConfig::set_property_spawn);
	ClassDB::bind_method(D_METHOD("set_property_sync", "path", "enabled"), &SceneReplicationConfig::set_property_sync);

	ClassDB::bind_method(D_METHOD("get_replication"), &SceneReplicationConfig::get_replication);
	ClassDB::bind_method(D_METHOD("set_replication", "replication"), &SceneReplicationConfig::set_replication);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "replication", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL), "set_replication", "get_replication");
}
