#ifndef WSL_CLIENT_PEER_H
#define WSL_CLIENT_PEER_H

#include "wsl_peer.h"

#include "core/crypto/crypto.h"
#include "core/io/stream_peer_tls.h"

#ifndef WEB_ENABLED

class WSLClientPeer : public WSLPeer::PeerData {
private:
	// Client
	int _in_buf_size = DEF_BUF_SHIFT;
	int _in_pkt_size = DEF_PKT_SHIFT;
	int _out_buf_size = DEF_BUF_SHIFT;
	int _out_pkt_size = DEF_PKT_SHIFT;
	State _state;
	Ref<StreamPeerBuffer> _handshake_buffer;
	Vector<String> _protocols;
	bool _handshaking = true;
	bool _pending_request = true;
	bool _is_client = false;
	Ref<StreamPeer> _connection;
	Ref<StreamPeerTCP> _tcp;
	String _key;
	String _host;
	uint16_t _port = 0;
	Array _ip_candidates;
	bool _use_tls = false;
	IP::ResolverID _resolver_id = IP::RESOLVER_INVALID_ID;
	bool verify_tls = true;
	Ref<X509Certificate> tls_cert;

	void _do_client_handshake();
	bool _verify_server_response(String &r_protocol);

	// TODO move up?
	int close_code = -1;
	String close_reason;
	void disconnect_from_host();

public:
	virtual Error poll() override;

	Error connect_to_host(String p_host, String p_path, uint16_t p_port, bool p_tls, const Vector<String> p_protocol = Vector<String>(), const Vector<String> p_custom_headers = Vector<String>());

	WSLClientPeer(){};
	~WSLClientPeer(){};
};

#endif // WEB_ENABLED

#endif // WSL_CLIENT_PEER_H
