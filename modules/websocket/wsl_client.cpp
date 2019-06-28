/*************************************************************************/
/*  wsl_client.cpp                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#ifndef JAVASCRIPT_ENABLED

#include "wsl_client.h"
#include "core/io/ip.h"
#include "core/project_settings.h"

void WSLClient::_do_handshake() {
	if (!_requested) {
		_requested = true;
		const CharString cs = _request.utf8();
		// TODO non-blocking handshake
		_connection->put_data((const uint8_t *)cs.get_data(), cs.size() - 1);
	} else {
		uint8_t byte = 0;
		int read = 0;

		while (true) {
			Error err = _connection->get_partial_data(&byte, 1, read);
			if (err == ERR_FILE_EOF) {
				// We got a disconnect.
				disconnect_from_host();
				_on_error();
				return;
			} else if (err != OK) {
				// Got some error.
				disconnect_from_host();
				_on_error();
				return;
			} else if (read != 1) {
				// Busy, wait next poll.
				break;
			}
			// TODO lots of allocs. Use a buffer.
			_response += byte;
			if (_response.size() > WSL_MAX_HEADER_SIZE) {
				// Header is too big
				disconnect_from_host();
				_on_error();
				ERR_EXPLAIN("Response headers too big");
				ERR_FAIL();
			}
			if (_response.ends_with("\r\n\r\n")) {
				// Response is over, verify headers and create peer.
				if (!_verify_headers()) {
					disconnect_from_host();
					_on_error();
					ERR_EXPLAIN("Invalid response headers");
					ERR_FAIL();
				}
				// Create peer.
				_peer = Ref<WSLPeer>(memnew(WSLPeer));
				_peer->make_context(this, _in_buf_size, _in_pkt_size, _out_buf_size, _out_pkt_size);
				_on_connect(""); // TODO protocol
			}
		}
	}
}

bool WSLClient::_verify_headers() {
	Vector<String> psa = _response.trim_suffix("\r\n\r\n").split("\r\n");
	int len = psa.size();
	if (len < 4) {
		ERR_EXPLAIN("Not enough response headers.");
		ERR_FAIL_V(false);
	}

	Vector<String> req = psa[0].split(" ", false);
	if (req.size() < 2) {
		ERR_EXPLAIN("Invalid protocol or status code.");
		ERR_FAIL_V(false);
	}
	// Wrong protocol
	if (req[0] != "HTTP/1.1" || req[1] != "101") {
		ERR_EXPLAIN("Invalid protocol or status code.");
		ERR_FAIL_V(false);
	}

	Map<String, String> headers;
	for (int i = 1; i < len; i++) {
		Vector<String> header = psa[i].split(":", false, 1);
		if (header.size() != 2) {
			ERR_EXPLAIN("Invalid header -> " + psa[i]);
			ERR_FAIL_V(false);
		}
		String name = header[0].to_lower();
		String value = header[1].strip_edges();
		if (headers.has(name))
			headers[name] += "," + value;
		else
			headers[name] = value;
	}

#define _WLS_EXPLAIN(NAME, VALUE) \
	ERR_EXPLAIN("Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'");
#define _WLS_CHECK(NAME, VALUE) \
	_WLS_EXPLAIN(NAME, VALUE);  \
	ERR_FAIL_COND_V(!headers.has(NAME) || headers[NAME].to_lower() != VALUE, false);
#define _WLS_CHECK_NC(NAME, VALUE) \
	_WLS_EXPLAIN(NAME, VALUE);     \
	ERR_FAIL_COND_V(!headers.has(NAME) || headers[NAME] != VALUE, false);
	_WLS_CHECK("connection", "upgrade");
	_WLS_CHECK("upgrade", "websocket");
	// TODO compute value for random key
	_WLS_CHECK_NC("sec-websocket-accept", "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
#undef _WLS_CHECK_NC
#undef _WLS_CHECK
#undef _WLS_EXPLAIN

	return true;
}

Error WSLClient::connect_to_host(String p_host, String p_path, uint16_t p_port, bool p_ssl, PoolVector<String> p_protocols) {

	ERR_FAIL_COND_V(_connection.is_valid(), ERR_ALREADY_IN_USE);

	IP_Address addr;

	if (!p_host.is_valid_ip_address()) {
		addr = IP::get_singleton()->resolve_hostname(p_host);
	} else {
		addr = p_host;
	}

	ERR_FAIL_COND_V(!addr.is_valid(), ERR_INVALID_PARAMETER);

	String port = "";
	if ((p_port != 80 && !p_ssl) || (p_port != 443 && p_ssl)) {
		port = ":" + itos(p_port);
	}

	Error err = _tcp->connect_to_host(addr, p_port);
	if (err != OK) {
		_on_error();
		_tcp->disconnect_from_host();
		return err;
	}
	_connection = _tcp;
	_use_ssl = p_ssl;
	_host = p_host;

	_key = "dGhlIHNhbXBsZSBub25jZQ=="; // FIXME randomize this
	// TODO custom extra headers (allow overriding this too?)
	_request = "GET " + p_path + " HTTP/1.1\r\n";
	_request += "Host: " + p_host + port + "\r\n";
	_request += "Upgrade: websocket\r\n";
	_request += "Connection: Upgrade\r\n";
	_request += "Sec-WebSocket-Key: " + _key + "\r\n";
	_request += "Sec-WebSocket-Version: 13\r\n";
	if (p_protocols.size() > 0) {
		_request += "Sec-WebSocket-Protocol: ";
		for (int i = 0; i < p_protocols.size(); i++) {
			if (i != 0)
				_request += ",";
			_request += p_protocols[i];
		}
		_request += "\r\n";
	}
	_request += "\r\n";

	return OK;
}

int WSLClient::get_max_packet_size() const {
	return (1 << _out_buf_size) - PROTO_SIZE;
}

void WSLClient::poll() {
	if (_connection.is_null())
		return; // Not connected.
	if (_peer.is_valid()) {
		_peer->poll();
		if (!_peer->is_connected_to_host()) {
			_on_disconnect(_peer->close_code != 0);
			disconnect_from_host();
		}
		return;
	}

	switch (_tcp->get_status()) {
		case StreamPeerTCP::STATUS_NONE:
			// Clean close
			_on_error();
			disconnect_from_host();
			break;
		case StreamPeerTCP::STATUS_CONNECTED: {
			Ref<StreamPeerSSL> ssl;
			if (_use_ssl) {
				if (_connection == _tcp) {
					// Start SSL handshake
					ssl = Ref<StreamPeerSSL>(StreamPeerSSL::create());
					ERR_EXPLAIN("SSL is not available in this build");
					ERR_FAIL_COND(ssl.is_null());
					ssl->set_blocking_handshake_enabled(false);
					if (ssl->connect_to_stream(_tcp, verify_ssl, _host) != OK) {
						_on_error();
						disconnect_from_host();
						return;
					}
					_connection = ssl;
				} else {
					ssl = static_cast<Ref<StreamPeerSSL> >(_connection);
					ERR_FAIL_COND(ssl.is_null()); // Bug?
					ssl->poll();
				}
				if (ssl->get_status() == StreamPeerSSL::STATUS_HANDSHAKING)
					return; // Need more polling.
				else if (ssl->get_status() != StreamPeerSSL::STATUS_CONNECTED) {
					_on_error();
					disconnect_from_host();
					return; // Error.
				}
			}
			// Do websocket handshake.
			_do_handshake();
		} break;
		case StreamPeerTCP::STATUS_ERROR:
			_on_error();
			disconnect_from_host();
			break;
		case StreamPeerTCP::STATUS_CONNECTING:
			break; // Wait for connection
	}
}

Ref<WebSocketPeer> WSLClient::get_peer(int p_peer_id) const {

	return _peer;
}

NetworkedMultiplayerPeer::ConnectionStatus WSLClient::get_connection_status() const {

	if (_peer.is_valid() && _peer->is_connected_to_host())
		return CONNECTION_CONNECTED;

	if (_tcp->is_connected_to_host())
		return CONNECTION_CONNECTING;

	return CONNECTION_DISCONNECTED;
}

void WSLClient::disconnect_from_host(int p_code, String p_reason) {

	if (_peer.is_valid()) {
		_peer->close(p_code, p_reason);
		_peer = Ref<WSLPeer>();
	}
	if (_connection.is_valid()) {
		_connection = Ref<StreamPeer>(NULL);
		_tcp->disconnect_from_host();
	}
	_request = "";
	_response = "";
	_key = "";
	_host = "";
	_use_ssl = false;
	_requested = false;
}

IP_Address WSLClient::get_connected_host() const {

	return IP_Address();
}

uint16_t WSLClient::get_connected_port() const {

	return 1025;
}

Error WSLClient::set_buffers(int p_in_buffer, int p_in_packets, int p_out_buffer, int p_out_packets) {
	ERR_EXPLAIN("Buffers sizes can only be set before listening or connecting");
	ERR_FAIL_COND_V(_ctx != NULL, FAILED);

	_in_buf_size = nearest_shift(p_in_buffer - 1) + 10;
	_in_pkt_size = nearest_shift(p_in_packets - 1);
	_out_buf_size = nearest_shift(p_out_buffer - 1) + 10;
	_out_pkt_size = nearest_shift(p_out_packets - 1);
	return OK;
}

WSLClient::WSLClient() {
	_in_buf_size = nearest_shift((int)GLOBAL_GET(WSC_IN_BUF) - 1) + 10;
	_in_pkt_size = nearest_shift((int)GLOBAL_GET(WSC_IN_PKT) - 1);
	_out_buf_size = nearest_shift((int)GLOBAL_GET(WSC_OUT_BUF) - 1) + 10;
	_out_pkt_size = nearest_shift((int)GLOBAL_GET(WSC_OUT_PKT) - 1);

	_ctx = NULL;
	_tcp.instance();
	_requested = false;
}

WSLClient::~WSLClient() {

	disconnect_from_host();
}

#endif // JAVASCRIPT_ENABLED
