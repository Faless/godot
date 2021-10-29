#include "scene/main/node.h"

class MultiplayerSynchronizer : public Node {
	GDCLASS(MultiplayerSynchronizer, Node);

private:
	bool auto_sync = true;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_auto_sync_enabled(bool p_enabled);
	bool is_auto_sync_enabled() const;
	Error sync();

	MultiplayerSynchronizer() {}
};
