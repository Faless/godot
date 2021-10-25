
#include "core/object/ref_counted.h"

#include "core/multiplayer/multiplayer.h"

#include "core/object/gdvirtual.gen.inc"
#include "core/object/script_language.h"
#include "core/variant/native_ptr.h"

class MultiplayerAPI;

class MultiplayerReplicationInterface : public RefCounted {
	GDCLASS(MultiplayerReplicationInterface, RefCounted);

private:
	MultiplayerAPI *multiplayer = nullptr;

	Error _do_send(int p_peer, const PackedByteArray &p_data, Multiplayer::TransferMode p_mode, int p_channel, int p_cmd);

	Vector<uint8_t> packet_cache;

public:
	void set_multiplayer(MultiplayerAPI *p_multiplayer);

	virtual Error on_spawn_send(Object *p_obj, int p_peer);
	virtual Error on_spawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len);
	virtual Error on_despawn_send(Object *p_obj, int p_peer);
	virtual Error on_despawn_receive(int p_from, const uint8_t *p_buffer, int p_buffer_len);

	// Send a spawn/despawn message to the given peer.
	Error send_spawn(const PackedByteArray &p_data, int p_peer = 0);
	Error send_despawn(const PackedByteArray &p_data, int p_peer = 0);

	// Send sync/sync_all message to the given peer.
	Error send_sync(const PackedByteArray &p_data, int p_peer = 0);

	/* GDExtension */
	// TODO optimized send/receive with pointers.
	GDVIRTUAL2R(int, _on_spawn_send, Object *, int);
	GDVIRTUAL3R(int, _on_spawn_receive, int, GDNativeConstPtr<const uint8_t>, int);
	GDVIRTUAL2R(int, _on_despawn_send, Object *, int);
	GDVIRTUAL3R(int, _on_despawn_receive, int, GDNativeConstPtr<const uint8_t>, int);

	MultiplayerReplicationInterface() {}
};
