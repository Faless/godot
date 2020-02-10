#include "scene/gui/tree.h"

#ifndef EDITOR_DEBUGGER_TREE_H
#define EDITOR_DEBUGGER_TREE_H

class EditorDebuggerTree : public Tree {

	GDCLASS(EditorDebuggerTree, Tree);

	enum ItemMenu {
		ITEM_MENU_COPY_ERROR,
		ITEM_MENU_SAVE_REMOTE_NODE,
		ITEM_MENU_COPY_NODE_PATH,
	};

	ObjectID inspected_object_id;
	bool updating_scene_tree;
	Set<ObjectID> unfold_cache;
	PopupMenu *item_menu;

	void _scene_tree_folded(Object *obj);
	void _scene_tree_selected();
	void _scene_tree_rmb_selected(const Vector2 &p_position);
	void _item_menu_id_pressed(int p_option);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	int update_scene_tree(TreeItem *parent, const Array &nodes, int current_index);
	EditorDebuggerTree();
};
#endif // EDITOR_DEBUGGER_TREE_H
