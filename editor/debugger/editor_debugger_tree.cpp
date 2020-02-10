#include "editor_debugger_tree.h"

#include "editor/editor_node.h"

EditorDebuggerRemoteTree::EditorDebuggerRemoteTree() {
	inspected_object_id = 0;
	updating_scene_tree = false;

	set_v_size_flags(SIZE_EXPAND_FILL);
	set_allow_rmb_select(true);

	// Popup
	item_menu = memnew(PopupMenu);
	item_menu->connect("id_pressed", this, "_item_menu_id_pressed");
	add_child(item_menu);
}

void EditorDebuggerRemoteTree::_notification(int p_what) {
	if (p_what == NOTIFICATION_POSTINITIALIZE) {
		connect("cell_selected", this, "_scene_tree_selected");
		connect("item_collapsed", this, "_scene_tree_folded");
		connect("item_rmb_selected", this, "_scene_tree_rmb_selected");
	}
}

void EditorDebuggerRemoteTree::_bind_methods() {
	WARN_PRINT("bind_methods");
	ClassDB::bind_method(D_METHOD("_scene_tree_selected"), &EditorDebuggerRemoteTree::_scene_tree_selected);
	ClassDB::bind_method(D_METHOD("_scene_tree_folded"), &EditorDebuggerRemoteTree::_scene_tree_folded);
	ClassDB::bind_method(D_METHOD("_scene_tree_rmb_selected"), &EditorDebuggerRemoteTree::_scene_tree_rmb_selected);
	ADD_SIGNAL(MethodInfo("inspect_object", PropertyInfo(Variant::INT, "object_id")));
}

void EditorDebuggerRemoteTree::_scene_tree_selected() {

	if (updating_scene_tree) {

		return;
	}
	TreeItem *item = get_selected();
	if (!item) {

		return;
	}

	inspected_object_id = item->get_metadata(0);

	emit_signal("inspect_object", inspected_object_id);
}

void EditorDebuggerRemoteTree::_scene_tree_folded(Object *obj) {

	if (updating_scene_tree) {

		return;
	}
	TreeItem *item = Object::cast_to<TreeItem>(obj);

	if (!item)
		return;

	ObjectID id = item->get_metadata(0);
	if (unfold_cache.has(id)) {
		unfold_cache.erase(id);
	} else {
		unfold_cache.insert(id);
	}
}

void EditorDebuggerRemoteTree::_scene_tree_rmb_selected(const Vector2 &p_position) {

	TreeItem *item = get_item_at_position(p_position);
	if (!item)
		return;

	item->select(0);

	item_menu->clear();
	item_menu->add_icon_item(get_icon("CreateNewSceneFrom", "EditorIcons"), TTR("Save Branch as Scene"), ITEM_MENU_SAVE_REMOTE_NODE);
	item_menu->add_icon_item(get_icon("CopyNodePath", "EditorIcons"), TTR("Copy Node Path"), ITEM_MENU_COPY_NODE_PATH);
	item_menu->set_global_position(get_global_mouse_position());
	item_menu->popup();
}

/// Populates inspect_scene_tree recursively given data in nodes.
/// Nodes is an array containing 4 elements for each node, it follows this pattern:
/// nodes[i] == number of direct children of this node
/// nodes[i + 1] == node name
/// nodes[i + 2] == node class
/// nodes[i + 3] == node instance id
///
/// Returns the number of items parsed in nodes from current_index.
///
/// Given a nodes array like [R,A,B,C,D,E] the following Tree will be generated, assuming
/// filter is an empty String, R and A child count are 2, B is 1 and C, D and E are 0.
///
/// R
/// |-A
/// | |-B
/// | | |-C
/// | |
/// | |-D
/// |
/// |-E
///
int EditorDebuggerRemoteTree::update_scene_tree(TreeItem *parent, const Array &nodes, int current_index) {
	bool was_updating = updating_scene_tree;
	updating_scene_tree = true;
	String filter = EditorNode::get_singleton()->get_scene_tree_dock()->get_filter();
	String item_text = nodes[current_index + 1];
	String item_type = nodes[current_index + 2];
	bool keep = filter.is_subsequence_ofi(item_text);

	TreeItem *item = create_item(parent);
	item->set_text(0, item_text);
	item->set_tooltip(0, TTR("Type:") + " " + item_type);
	ObjectID id = ObjectID(nodes[current_index + 3]);
	Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(nodes[current_index + 2], "");
	if (icon.is_valid()) {
		item->set_icon(0, icon);
	}
	item->set_metadata(0, id);

	if (id == inspected_object_id) {
		TreeItem *cti = item->get_parent();
		while (cti) {
			cti->set_collapsed(false);
			cti = cti->get_parent();
		}
		item->select(0);
	}

	// Set current item as collapsed if necessary
	if (parent) {
		if (!unfold_cache.has(id)) {
			item->set_collapsed(true);
		}
	}

	int children_count = nodes[current_index];
	// Tracks the total number of items parsed in nodes, this is used to skips nodes that
	// are not direct children of the current node since we can't know in advance the total
	// number of children, direct and not, of a node without traversing the nodes array previously.
	// Keeping track of this allows us to build our remote scene tree by traversing the node
	// array just once.
	int items_count = 1;
	for (int i = 0; i < children_count; i++) {
		// Called for each direct child of item.
		// Direct children of current item might not be adjacent so items_count must
		// be incremented by the number of items parsed until now, otherwise we would not
		// be able to access the next child of the current item.
		// items_count is multiplied by 4 since that's the number of elements in the nodes
		// array needed to represent a single node.
		items_count += update_scene_tree(item, nodes, current_index + items_count * 4);
	}

	// If item has not children and should not be kept delete it
	if (!keep && !item->get_children() && parent) {
		parent->remove_child(item);
		memdelete(item);
	}

	if (!was_updating) {
		updating_scene_tree = false;
	}
	return items_count;
}

void EditorDebuggerRemoteTree::_item_menu_id_pressed(int p_option) {

	switch (p_option) {

		case ITEM_MENU_SAVE_REMOTE_NODE: {

			//			file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
			//			file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
			//			file_dialog_mode = SAVE_NODE;
			//
			//			List<String> extensions;
			//			Ref<PackedScene> sd = memnew(PackedScene);
			//			ResourceSaver::get_recognized_extensions(sd, &extensions);
			//			file_dialog->clear_filters();
			//			for (int i = 0; i < extensions.size(); i++) {
			//				file_dialog->add_filter("*." + extensions[i] + " ; " + extensions[i].to_upper());
			//			}

			//			file_dialog->popup_centered_ratio();
		} break;
		case ITEM_MENU_COPY_NODE_PATH: {

			TreeItem *ti = get_selected();
			String text = ti->get_text(0);

			if (ti->get_parent() == NULL) {
				text = ".";
			} else if (ti->get_parent()->get_parent() == NULL) {
				text = ".";
			} else {
				while (ti->get_parent()->get_parent() != get_root()) {
					ti = ti->get_parent();
					text = ti->get_text(0) + "/" + text;
				}
			}

			OS::get_singleton()->set_clipboard(text);
		} break;
	}
}
