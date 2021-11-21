/*************************************************************************/
/*  multiplayer_spawner.h                                                */
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

#ifndef MULTIPLAYER_SPAWNER_H
#define MULTIPLAYER_SPAWNER_H

#include "scene/main/node.h"

#include "core/variant/typed_array.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/scene_replication_config.h"

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

private:
	TypedArray<PackedScene> spawnable_scenes;
	Set<ResourceUID::ID> spawnable_ids;
	NodePath spawn_path;

	Set<ObjectID> tracked_nodes;
	Set<ObjectID> remote_nodes;
	bool auto_spawn = false;

	void _connect_node(Node *p_node);
	void _node_exit(ObjectID p_id);

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _node_added(Node *p_node);

public:
	bool can_spawn(const ResourceUID::ID &p_id) const { return spawnable_ids.has(p_id); }
	TypedArray<PackedScene> get_spawnable_scenes();
	void set_spawnable_scenes(TypedArray<PackedScene> p_scenes);
	NodePath get_spawn_path() const;
	void set_spawn_path(const NodePath &p_path);
	bool is_auto_spawning() const;
	void set_auto_spawning(bool p_enabled);

	Node *get_currently_spawning();

	Node *spawn_custom(const Variant &p_data, int p_peer);
	Error spawn(Node *p_node, int p_peer);
	bool is_tracking(const Node *p_node) const;
	void track(Node *p_node);

	Error remote_spawn(Node *p_node, const String &p_name);
	Error remote_despawn(Node *p_node);

	GDVIRTUAL1RC(Object *, _spawn_custom, const Variant &);
	GDVIRTUAL1RC(bool, _can_spawn_scene, const Variant &);

	MultiplayerSpawner() {}
};

#endif // MULTIPLAYER_SPAWNER_H
