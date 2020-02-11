#include "editor_debugger_tree.h"

#include "editor/editor_node.h"
#include "scene/debugger/scene_debugger.h"

EditorDebuggerTree::EditorDebuggerTree() {
	inspected_object_id = 0;
	updating_scene_tree = false;

	set_v_size_flags(SIZE_EXPAND_FILL);
	set_allow_rmb_select(true);

	// Popup
	item_menu = memnew(PopupMenu);
	item_menu->connect("id_pressed", this, "_item_menu_id_pressed");
	add_child(item_menu);
}

void EditorDebuggerTree::_notification(int p_what) {
	if (p_what == NOTIFICATION_POSTINITIALIZE) {
		connect("cell_selected", this, "_scene_tree_selected");
		connect("item_collapsed", this, "_scene_tree_folded");
		connect("item_rmb_selected", this, "_scene_tree_rmb_selected");
	}
}

void EditorDebuggerTree::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_scene_tree_selected"), &EditorDebuggerTree::_scene_tree_selected);
	ClassDB::bind_method(D_METHOD("_scene_tree_folded"), &EditorDebuggerTree::_scene_tree_folded);
	ClassDB::bind_method(D_METHOD("_scene_tree_rmb_selected"), &EditorDebuggerTree::_scene_tree_rmb_selected);
	ADD_SIGNAL(MethodInfo("object_selected", PropertyInfo(Variant::INT, "object_id")));
}

void EditorDebuggerTree::_scene_tree_selected() {

	if (updating_scene_tree) {

		return;
	}
	TreeItem *item = get_selected();
	if (!item) {

		return;
	}

	inspected_object_id = item->get_metadata(0);

	emit_signal("object_selected", inspected_object_id);
}

void EditorDebuggerTree::_scene_tree_folded(Object *obj) {

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

void EditorDebuggerTree::_scene_tree_rmb_selected(const Vector2 &p_position) {

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

/// Populates inspect_scene_tree given data in nodes as a flat list, encoded depth first.
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
void EditorDebuggerTree::update_scene_tree(const SceneDebuggerTree *p_tree) {
	updating_scene_tree = true;
	String filter = EditorNode::get_singleton()->get_scene_tree_dock()->get_filter();

	// Nodes are in a flatten list, depth first. Use a stack of parents, avoid recursion.
	List<Pair<TreeItem *, int> > parents;
	for (int i = 0; i < p_tree->nodes.size(); i++) {
		TreeItem *parent = NULL;
		if (parents.size()) { // Find last parent.
			Pair<TreeItem *, int> &p = parents[0];
			parent = p.first;
			if (!(--p.second)) { // If no child left, remove it.
				parents.pop_front();
			}
		}
		// Add this node.
		const SceneDebuggerTree::RemoteNode &node = p_tree->nodes[i];
		TreeItem *item = create_item(parent);
		item->set_text(0, node.type_name);
		item->set_tooltip(0, TTR("Type:") + " " + node.type_name);
		Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(node.type_name, "");
		if (icon.is_valid()) {
			item->set_icon(0, icon);
		}
		item->set_metadata(0, node.id);

		// Set current item as collapsed if necessary (root is never collapsed)
		if (parent) {
			if (!unfold_cache.has(node.id)) {
				item->set_collapsed(true);
			}
		}
		if (node.id == inspected_object_id) {
			item->select(0);
		}
		// TODO filter list...

		// Add in front of the parents stack if children are expected.
		if (node.child_count) {
			parents.push_front(Pair<TreeItem *, int>(item, node.child_count));
		}
	}
	updating_scene_tree = false;
}

void EditorDebuggerTree::_item_menu_id_pressed(int p_option) {

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
