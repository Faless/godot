#include "scene/main/node.h"

class MultiplayerSpawner : public Node {
	GDCLASS(MultiplayerSpawner, Node);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	Error spawn(Node *p_node, const PackedByteArray &p_data);
	MultiplayerSpawner();
};
