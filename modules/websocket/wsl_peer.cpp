/*************************************************************************/
/*  wsl_peer.cpp                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
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

#ifndef WEB_ENABLED

#include "wsl_peer.h"

#include "wsl_client.h"
#include "wsl_client_peer.h"
#include "wsl_server.h"
#include "wsl_server_peer.h"

#include "core/crypto/crypto_core.h"
#include "core/math/random_number_generator.h"
#include "core/os/os.h"

///
/// Server functions
///
Error WSLContext::accept_stream(Ref<StreamPeer> p_stream, const Vector<String> &p_protocols, const Vector<String> &p_custom_headers) {
	ERR_FAIL_COND_V(p_stream.is_null(), ERR_INVALID_PARAMETER);

	if (p_stream->is_class_ptr(StreamPeerTCP::get_class_ptr_static())) {
		_connection = p_stream;
		_use_tls = false;
	} else if (p_stream->is_class_ptr(StreamPeerTLS::get_class_ptr_static())) {
		_connection = p_stream;
		_use_tls = true;
	}
	ERR_FAIL_COND_V(!_connection.is_valid(), ERR_INVALID_PARAMETER);
	_pending_request = true;
	_protocols = p_protocols;
	_custom_headers = p_custom_headers;
	_state = WebSocketPeer::STATE_CONNECTING;
	return OK;
}

bool WSLContext::_parse_client_request(const Vector<String> p_protocols, String &r_resource_name) {
	Vector<String> psa = String((const char *)_handshake_buffer->get_data_array().ptr()).split("\r\n");
	int len = psa.size();
	ERR_FAIL_COND_V_MSG(len < 4, false, "Not enough response headers, got: " + itos(len) + ", expected >= 4.");

	Vector<String> req = psa[0].split(" ", false);
	ERR_FAIL_COND_V_MSG(req.size() < 2, false, "Invalid protocol or status code.");

	// Wrong protocol
	ERR_FAIL_COND_V_MSG(req[0] != "GET" || req[2] != "HTTP/1.1", false, "Invalid method or HTTP version.");

	r_resource_name = req[1];
	HashMap<String, String> headers;
	for (int i = 1; i < len; i++) {
		Vector<String> header = psa[i].split(":", false, 1);
		ERR_FAIL_COND_V_MSG(header.size() != 2, false, "Invalid header -> " + psa[i]);
		String name = header[0].to_lower();
		String value = header[1].strip_edges();
		if (headers.has(name)) {
			headers[name] += "," + value;
		} else {
			headers[name] = value;
		}
	}
#define WSL_CHECK(NAME, VALUE)                                                          \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME) || headers[NAME].to_lower() != VALUE, false, \
			"Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'.");
#define WSL_CHECK_EX(NAME) \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME), false, "Missing header '" + String(NAME) + "'.");
	WSL_CHECK("upgrade", "websocket");
	WSL_CHECK("sec-websocket-version", "13");
	WSL_CHECK_EX("sec-websocket-key");
	WSL_CHECK_EX("connection");
#undef WSL_CHECK_EX
#undef WSL_CHECK
	_key = headers["sec-websocket-key"];
	if (headers.has("sec-websocket-protocol")) {
		Vector<String> protos = headers["sec-websocket-protocol"].split(",");
		for (int i = 0; i < protos.size(); i++) {
			String proto = protos[i].strip_edges();
			// Check if we have the given protocol
			for (int j = 0; j < p_protocols.size(); j++) {
				if (proto != p_protocols[j]) {
					continue;
				}
				_protocol = proto;
				break;
			}
			// Found a protocol
			if (!_protocol.is_empty()) {
				break;
			}
		}
		if (_protocol.is_empty()) { // Invalid protocol(s) requested
			return false;
		}
	} else if (p_protocols.size() > 0) { // No protocol requested, but we need one
		return false;
	}
	return true;
}

Error WSLContext::_do_server_handshake(const Vector<String> p_protocols, String &r_resource_name, const Vector<String> &p_extra_headers) {
	if (_use_tls) {
		Ref<StreamPeerTLS> tls = static_cast<Ref<StreamPeerTLS>>(_connection);
		if (tls.is_null()) {
			ERR_FAIL_V_MSG(ERR_BUG, "Couldn't get StreamPeerTLS for WebSocket handshake.");
			_state = WebSocketPeer::STATE_CLOSED;
			return FAILED;
		}
		tls->poll();
		if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
			return OK; // Pending handshake
		} else if (tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
			print_verbose(vformat("WebSocket SSL connection error during handshake (StreamPeerTLS status code %d).", tls->get_status()));
			_state = WebSocketPeer::STATE_CLOSED;
			return FAILED;
		}
	}

	if (_pending_request) {
		int read = 0;
		while (true) {
			ERR_FAIL_COND_V_MSG(_handshake_buffer->get_available_bytes() < 1, ERR_OUT_OF_MEMORY, "WebSocket response headers are too big.");
			int pos = _handshake_buffer->get_position();
			Vector<uint8_t> data = _handshake_buffer->get_data_array();
			Error err = _connection->get_partial_data(data.ptrw() + pos, 1, read);
			if (err != OK) { // Got an error
				print_verbose(vformat("WebSocket error while getting partial data (StreamPeer error code %d).", err));
				_state = WebSocketPeer::STATE_CLOSED;
				return FAILED;
			} else if (read != 1) { // Busy, wait next poll
				return OK;
			}
			char *r = (char *)data.ptr();
			int l = pos;
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				r[l - 3] = '\0';
				if (!_parse_client_request(p_protocols, r_resource_name)) {
					_state = WebSocketPeer::STATE_CLOSED;
					return FAILED;
				}
				String s = "HTTP/1.1 101 Switching Protocols\r\n";
				s += "Upgrade: websocket\r\n";
				s += "Connection: Upgrade\r\n";
				s += "Sec-WebSocket-Accept: " + WSLPeer::compute_key_response(_key) + "\r\n";
				if (!_protocol.is_empty()) {
					s += "Sec-WebSocket-Protocol: " + _protocol + "\r\n";
				}
				for (int i = 0; i < p_extra_headers.size(); i++) {
					s += p_extra_headers[i] + "\r\n";
				}
				s += "\r\n";
				CharString cs = s.utf8();
				_handshake_buffer->clear();
				_handshake_buffer->put_data((const uint8_t *)cs.get_data(), cs.size());
				_handshake_buffer->seek(0);
				_pending_request = false;
				break;
			}
			_handshake_buffer->seek(pos + 1);
		}
	}

	if (_pending_request) { // Still pending.
		return OK;
	}

	int left = _handshake_buffer->get_available_bytes();
	if (left) {
		Vector<uint8_t> data = _handshake_buffer->get_data_array();
		int pos = _handshake_buffer->get_position();
		int sent = 0;
		Error err = _connection->put_partial_data(data.ptr() + pos, left, sent);
		if (err != OK) {
			print_verbose(vformat("WebSocket error while putting partial data (StreamPeer error code %d).", err));
			_state = WebSocketPeer::STATE_CLOSED;
			return err;
		}
		_handshake_buffer->seek(pos + sent);
		left -= sent;
		if (left == 0) {
			_state = WebSocketPeer::STATE_OPEN;
		}
	}

	return OK;
}

///
/// Client functions
///
Error WSLContext::_client_poll() {
	Ref<StreamPeerTCP> tcp = get_tcp();
	ERR_FAIL_COND_V(tcp.is_null(), ERR_BUG);
	if (_resolver_id != IP::RESOLVER_INVALID_ID) {
		IP::ResolverStatus ip_status = IP::get_singleton()->get_resolve_item_status(_resolver_id);
		if (ip_status == IP::RESOLVER_STATUS_WAITING) {
			return OK;
		}
		// Anything else is either a candidate or a failure.
		Error err = FAILED;
		if (ip_status == IP::RESOLVER_STATUS_DONE) {
			_ip_candidates = IP::get_singleton()->get_resolve_item_addresses(_resolver_id);
			while (_ip_candidates.size()) {
				err = tcp->connect_to_host(_ip_candidates.pop_front(), _port);
				if (err == OK) {
					break;
				}
			}
		}
		IP::get_singleton()->erase_resolve_item(_resolver_id);
		_resolver_id = IP::RESOLVER_INVALID_ID;
		if (err != OK) {
			_state = WebSocketPeer::STATE_CLOSED;
			return FAILED;
		}
	}

	if (_connection.is_null()) {
		return OK; // Not connected.
	}

	tcp->poll();
	switch (tcp->get_status()) {
		case StreamPeerTCP::STATUS_NONE:
			// Clean close
			_state = WebSocketPeer::STATE_CLOSED;
			return OK;
		case StreamPeerTCP::STATUS_CONNECTED: {
			_ip_candidates.clear();
			Ref<StreamPeerTLS> tls;
			if (_use_tls) {
				if (_connection == tcp) {
					// Start SSL handshake
					tls = Ref<StreamPeerTLS>(StreamPeerTLS::create());
					ERR_FAIL_COND_V_MSG(tls.is_null(), ERR_BUG, "SSL is not available in this build.");
					tls->set_blocking_handshake_enabled(false);
					if (tls->connect_to_stream(tcp, verify_tls, _host, tls_cert) != OK) {
						_state = WebSocketPeer::STATE_CLOSED;
						return FAILED;
					}
					_connection = tls;
				} else {
					tls = static_cast<Ref<StreamPeerTLS>>(_connection);
					ERR_FAIL_COND_V(tls.is_null(), ERR_BUG);
					tls->poll();
				}
				if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
					return OK; // Need more polling.
				} else if (tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
					_state = WebSocketPeer::STATE_CLOSED;
					return FAILED; // Error.
				}
			}
			// Do websocket handshake.
			_do_client_handshake();
			return OK;
		}
		case StreamPeerTCP::STATUS_ERROR:
			while (_ip_candidates.size() > 0) {
				tcp->disconnect_from_host();
				if (tcp->connect_to_host(_ip_candidates.pop_front(), _port) == OK) {
					return OK;
				}
			}
			_state = WebSocketPeer::STATE_CLOSED;
			return FAILED;
		case StreamPeerTCP::STATUS_CONNECTING:
			return OK;
	}
	return OK;
}

void WSLContext::_do_client_handshake() {
	if (_pending_request) {
		int left = _handshake_buffer->get_available_bytes();
		int pos = _handshake_buffer->get_position();
		const Vector<uint8_t> data = _handshake_buffer->get_data_array();
		int sent = 0;
		Error err = _connection->put_partial_data(data.ptr() + pos, left, sent);
		// Sending handshake failed
		if (err != OK) {
			// TODO err
			_state = WebSocketPeer::STATE_CLOSED;
			return;
		}
		_handshake_buffer->seek(pos + sent);
		if (_handshake_buffer->get_available_bytes() == 0) {
			_pending_request = false;
			_handshake_buffer->clear();
			_handshake_buffer->resize(WSL_MAX_HEADER_SIZE);
			_handshake_buffer->seek(0);
		}
	} else {
		int read = 0;
		while (true) {
			int left = _handshake_buffer->get_available_bytes();
			int pos = _handshake_buffer->get_position();
			Vector<uint8_t> data = _handshake_buffer->get_data_array();
			if (left == 0) {
				// Header is too big
				_state = WebSocketPeer::STATE_CLOSED;
				ERR_FAIL_MSG("Response headers too big.");
				return;
			}

			Error err = _connection->get_partial_data(data.ptrw() + pos, 1, read);
			if (err == ERR_FILE_EOF) {
				// We got a disconnect.
				_state = WebSocketPeer::STATE_CLOSED;
				return;
			} else if (err != OK) {
				// Got some error.
				_state = WebSocketPeer::STATE_CLOSED;
				return;
			} else if (read != 1) {
				// Busy, wait next poll.
				break;
			}
			_handshake_buffer->seek(pos + read);

			// Check "\r\n\r\n" header terminator
			char *r = (char *)data.ptrw();
			int l = pos;
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				r[l - 3] = '\0';
				String protocol;
				// Response is over, verify headers and create peer.
				if (!_verify_server_response(protocol)) {
					_state = WebSocketPeer::STATE_CLOSED;
					ERR_FAIL_MSG("Invalid response headers.");
				}
				// Create peer. TODO check
				// TODO FIXME
				// make_context(data, _in_buf_size, _in_pkt_size, _out_buf_size, _out_pkt_size);
				//set_no_delay(true);
				_state = WebSocketPeer::STATE_OPEN;
				break;
			}
		}
	}
}

bool WSLContext::_verify_server_response(String &r_protocol) {
	Vector<uint8_t> data = _handshake_buffer->get_data_array();
	String s = (char *)data.ptrw();
	Vector<String> psa = s.split("\r\n");
	int len = psa.size();
	ERR_FAIL_COND_V_MSG(len < 4, false, "Not enough response headers. Got: " + itos(len) + ", expected >= 4.");

	Vector<String> req = psa[0].split(" ", false);
	ERR_FAIL_COND_V_MSG(req.size() < 2, false, "Invalid protocol or status code. Got '" + psa[0] + "', expected 'HTTP/1.1 101'.");

	// Wrong protocol
	ERR_FAIL_COND_V_MSG(req[0] != "HTTP/1.1", false, "Invalid protocol. Got: '" + req[0] + "', expected 'HTTP/1.1'.");
	ERR_FAIL_COND_V_MSG(req[1] != "101", false, "Invalid status code. Got: '" + req[1] + "', expected '101'.");

	HashMap<String, String> headers;
	for (int i = 1; i < len; i++) {
		Vector<String> header = psa[i].split(":", false, 1);
		ERR_FAIL_COND_V_MSG(header.size() != 2, false, "Invalid header -> " + psa[i] + ".");
		String name = header[0].to_lower();
		String value = header[1].strip_edges();
		if (headers.has(name)) {
			headers[name] += "," + value;
		} else {
			headers[name] = value;
		}
	}

#define WSL_CHECK(NAME, VALUE)                                                          \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME) || headers[NAME].to_lower() != VALUE, false, \
			"Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'.");
#define WSL_CHECK_NC(NAME, VALUE)                                            \
	ERR_FAIL_COND_V_MSG(!headers.has(NAME) || headers[NAME] != VALUE, false, \
			"Missing or invalid header '" + String(NAME) + "'. Expected value '" + VALUE + "'.");
	WSL_CHECK("connection", "upgrade");
	WSL_CHECK("upgrade", "websocket");
	WSL_CHECK_NC("sec-websocket-accept", WSLPeer::compute_key_response(_key));
#undef WSL_CHECK_NC
#undef WSL_CHECK
	if (_protocols.size() == 0) {
		// We didn't request a custom protocol
		ERR_FAIL_COND_V_MSG(headers.has("sec-websocket-protocol"), false, "Received unrequested sub-protocol -> " + headers["sec-websocket-protocol"]);
	} else {
		// We requested at least one custom protocol but didn't receive one
		ERR_FAIL_COND_V_MSG(!headers.has("sec-websocket-protocol"), false, "Requested sub-protocol(s) but received none.");
		// Check received sub-protocol was one of those requested.
		r_protocol = headers["sec-websocket-protocol"];
		bool valid = false;
		for (int i = 0; i < _protocols.size(); i++) {
			if (_protocols[i] != r_protocol) {
				continue;
			}
			valid = true;
			break;
		}
		if (!valid) {
			ERR_FAIL_V_MSG(false, "Received unrequested sub-protocol -> " + r_protocol);
			return false;
		}
	}
	return true;
}

Error WSLContext::connect_to_host(String p_host, String p_path, uint16_t p_port, bool p_tls, const Vector<String> p_protocols, const Vector<String> p_custom_headers, bool p_verify_tls, Ref<X509Certificate> p_cert) {
	ERR_FAIL_COND_V(_connection.is_valid(), ERR_ALREADY_IN_USE);
	ERR_FAIL_COND_V(p_path.is_empty(), ERR_INVALID_PARAMETER);

	tls_cert = p_cert;
	verify_tls = p_verify_tls;
	if (p_host.is_valid_ip_address()) {
		_ip_candidates.push_back(IPAddress(p_host));
	} else {
		// Queue hostname for resolution.
		_resolver_id = IP::get_singleton()->resolve_hostname_queue_item(p_host);
		ERR_FAIL_COND_V(_resolver_id == IP::RESOLVER_INVALID_ID, ERR_INVALID_PARAMETER);
		// Check if it was found in cache.
		IP::ResolverStatus ip_status = IP::get_singleton()->get_resolve_item_status(_resolver_id);
		if (ip_status == IP::RESOLVER_STATUS_DONE) {
			_ip_candidates = IP::get_singleton()->get_resolve_item_addresses(_resolver_id);
			IP::get_singleton()->erase_resolve_item(_resolver_id);
			_resolver_id = IP::RESOLVER_INVALID_ID;
		}
	}

	Ref<StreamPeerTCP> tcp;
	tcp.instantiate();
	// We assume OK while hostname resolution is pending.
	Error err = _resolver_id != IP::RESOLVER_INVALID_ID ? OK : FAILED;
	while (_ip_candidates.size()) {
		err = tcp->connect_to_host(_ip_candidates.pop_front(), p_port);
		if (err == OK) {
			break;
		}
	}
	if (err != OK) {
		tcp->disconnect_from_host();
		return err;
	}
	_connection = tcp;
	_use_tls = p_tls;
	_host = p_host;
	_port = p_port;
	// Strip edges from protocols.
	_protocols.resize(p_protocols.size());
	String *pw = _protocols.ptrw();
	for (int i = 0; i < p_protocols.size(); i++) {
		pw[i] = p_protocols[i].strip_edges();
	}

	_key = WSLPeer::generate_key();
	String request = "GET " + p_path + " HTTP/1.1\r\n";
	String port = "";
	if ((p_port != 80 && !p_tls) || (p_port != 443 && p_tls)) {
		port = ":" + itos(p_port);
	}
	request += "Host: " + p_host + port + "\r\n";
	request += "Upgrade: websocket\r\n";
	request += "Connection: Upgrade\r\n";
	request += "Sec-WebSocket-Key: " + _key + "\r\n";
	request += "Sec-WebSocket-Version: 13\r\n";
	if (p_protocols.size() > 0) {
		request += "Sec-WebSocket-Protocol: ";
		for (int i = 0; i < p_protocols.size(); i++) {
			if (i != 0) {
				request += ",";
			}
			request += p_protocols[i];
		}
		request += "\r\n";
	}
	for (int i = 0; i < p_custom_headers.size(); i++) {
		request += p_custom_headers[i] + "\r\n";
	}
	request += "\r\n";
	CharString cs = request.utf8();
	_handshake_buffer->put_data((const uint8_t *)cs.get_data(), cs.size());
	_state = WebSocketPeer::STATE_CONNECTING;

	return OK;
}

///
/// Callback functions.
///
void WSLContext::_wsl_poll() {
	ERR_FAIL_COND(!_ctx);
	int err = 0;
	if ((err = wslay_event_recv(_ctx)) != 0 || (err = wslay_event_send(_ctx)) != 0) {
		print_verbose("Websocket (wslay) poll error: " + itos(err));
		wslay_event_context_free(_ctx);
		_ctx = nullptr;
	} else if (wslay_event_get_close_sent(_ctx) && wslay_event_get_close_received(_ctx)) {
		wslay_event_context_free(_ctx);
		_ctx = nullptr;
	}
}

ssize_t WSLContext::_wsl_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {
	WSLContext *peer_data = (WSLContext *)user_data;
	if (!peer_data->is_active()) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	Ref<StreamPeer> conn = peer_data->_connection;
	int read = 0;
	Error err = conn->get_partial_data(data, len, read);
	if (err != OK) {
		print_verbose("Websocket get data error: " + itos(err) + ", read (should be 0!): " + itos(read));
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	if (read == 0) {
		wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		return -1;
	}
	return read;
}

ssize_t WSLContext::_wsl_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
	WSLContext *peer_data = (WSLContext *)user_data;
	if (!peer_data->is_active()) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	Ref<StreamPeer> conn = peer_data->_connection;
	int sent = 0;
	Error err = conn->put_partial_data(data, len, sent);
	if (err != OK) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	if (sent == 0) {
		wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		return -1;
	}
	return sent;
}

int WSLContext::_wsl_genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
	RandomNumberGenerator rng;
	// TODO maybe use crypto in the future?
	rng.set_seed(OS::get_singleton()->get_unix_time());
	for (unsigned int i = 0; i < len; i++) {
		buf[i] = (uint8_t)rng.randi_range(0, 255);
	}
	return 0;
}

void WSLContext::_wsl_msg_recv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
	WSLContext *peer_data = (WSLContext *)user_data;
	if (!peer_data->is_active() || peer_data->_state == WebSocketPeer::STATE_CLOSING) {
		return;
	}
	WSLPeer *peer = static_cast<WSLPeer *>(peer_data->peer);

	if (peer->parse_message(arg) != OK) {
		return;
	}

	// TODO Nothing?
#if 0
	if (peer_data->is_server) {
		WSLServer *helper = static_cast<WSLServer *>(peer_data->obj);
		helper->_on_peer_packet(peer_data->id);
	} else {
		WSLClient *helper = static_cast<WSLClient *>(peer_data->obj);
		helper->_on_peer_packet();
	}
#endif
}

wslay_event_callbacks WSLContext::_wsl_callbacks = {
	_wsl_recv_callback,
	_wsl_send_callback,
	_wsl_genmask_callback,
	nullptr, /* on_frame_recv_start_callback */
	nullptr, /* on_frame_recv_callback */
	nullptr, /* on_frame_recv_end_callback */
	_wsl_msg_recv_callback
};

void WSLContext::close(int p_code, String p_reason) {
	if (_ctx && !wslay_event_get_close_sent(_ctx)) {
		CharString cs = p_reason.utf8();
		wslay_event_queue_close(_ctx, p_code, (uint8_t *)cs.ptr(), cs.size());
		wslay_event_send(_ctx);
		_state = WebSocketPeer::STATE_CLOSING;
	}
}

Ref<StreamPeerTCP> WSLContext::get_tcp() const {
	ERR_FAIL_COND_V(_connection.is_null(), Ref<StreamPeerTCP>());
	if (_connection->is_class_ptr(StreamPeerTCP::get_class_ptr_static())) {
		return static_cast<Ref<StreamPeerTCP>>(_connection);
	} else if (_connection->is_class_ptr(StreamPeerTLS::get_class_ptr_static())) {
		Ref<StreamPeerTLS> tls = static_cast<Ref<StreamPeerTLS>>(_connection);
		Ref<StreamPeer> stream = tls->get_stream();
		ERR_FAIL_COND_V(stream.is_null() || !stream->is_class_ptr(StreamPeerTCP::get_class_ptr_static()), Ref<StreamPeerTCP>());
		return static_cast<Ref<StreamPeerTCP>>(stream);
	}
	ERR_FAIL_V(Ref<StreamPeerTCP>());
}

void WSLContext::make_context(bool p_is_server, unsigned int p_max_recv_msg_length) {
	ERR_FAIL_COND(_ctx != nullptr);

	if (p_is_server) {
		wslay_event_context_server_init(&_ctx, &_wsl_callbacks, this);
	} else {
		wslay_event_context_client_init(&_ctx, &_wsl_callbacks, this);
	}
	wslay_event_config_set_max_recv_msg_length(_ctx, p_max_recv_msg_length);
}

Error WSLPeer::connect_to_url(String p_url, const Vector<String> p_protocols, const Vector<String> p_custom_headers, bool p_verify_tls, Ref<X509Certificate> p_cert) {
	ERR_FAIL_COND_V(_wsl_context.is_valid(), ERR_ALREADY_IN_USE);
	String host = p_url;
	String path;
	String scheme;
	int port = 0;
	Error err = p_url.parse_url(scheme, host, port, path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Invalid URL: " + p_url);

	bool tls = false;
	if (scheme == "wss://") {
		tls = true;
	}
	if (port == 0) {
		port = tls ? 443 : 80;
	}
	if (path.is_empty()) {
		path = "/";
	}
	WSLContext *data = memnew(WSLContext);
	err = data->connect_to_host(host, path, port, tls, p_protocols, p_custom_headers, p_verify_tls, p_cert);
	if (err != OK) {
		memdelete(data);
		data = nullptr;
	}
	_wsl_context = Ref<WSLContext>(data);
	return err;
}

Error WSLPeer::accept_stream(Ref<StreamPeer> p_stream, const Vector<String> p_protocols, const Vector<String> p_custom_headers) {
	ERR_FAIL_COND_V(_wsl_context.is_valid(), ERR_ALREADY_IN_USE);
	WSLContext *data = memnew(WSLContext);
	// TODO FIXME meh..
	Error err = data->accept_stream(p_stream, p_protocols, p_custom_headers);
	if (err != OK) {
		memdelete(data);
		data = nullptr;
	}
	_wsl_context = Ref<WSLContext>(data);
	return err;
}

String WSLPeer::generate_key() {
	// Random key
	RandomNumberGenerator rng;
	rng.set_seed(OS::get_singleton()->get_unix_time());
	Vector<uint8_t> bkey;
	int len = 16; // 16 bytes, as per RFC
	bkey.resize(len);
	uint8_t *w = bkey.ptrw();
	for (int i = 0; i < len; i++) {
		w[i] = (uint8_t)rng.randi_range(0, 255);
	}
	return CryptoCore::b64_encode_str(&w[0], len);
}

String WSLPeer::compute_key_response(String p_key) {
	String key = p_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // Magic UUID as per RFC
	Vector<uint8_t> sha = key.sha1_buffer();
	return CryptoCore::b64_encode_str(sha.ptr(), sha.size());
}

Error WSLPeer::parse_message(const wslay_event_on_msg_recv_arg *arg) {
	uint8_t is_string = 0;
	if (arg->opcode == WSLAY_TEXT_FRAME) {
		is_string = 1;
	} else if (arg->opcode == WSLAY_CONNECTION_CLOSE) {
		close_code = arg->status_code;
		size_t len = arg->msg_length;
		close_reason = "";
		if (len > 2 /* first 2 bytes = close code */) {
			close_reason.parse_utf8((char *)arg->msg + 2, len - 2);
		}
		if (!wslay_event_get_close_sent(_wsl_context->get_ctx())) {
			// TODO FIXME close request
#if 0
			if (_data->is_server) {
				WSLServer *helper = static_cast<WSLServer *>(_data->obj);
				helper->_on_close_request(_data->id, close_code, close_reason);
			} else {
				WSLClient *helper = static_cast<WSLClient *>(_data->obj);
				helper->_on_close_request(close_code, close_reason);
			}
#endif
		}
		return ERR_FILE_EOF;
	} else if (arg->opcode != WSLAY_BINARY_FRAME) {
		// Ping or pong
		return ERR_SKIP;
	}
	_in_buffer.write_packet(arg->msg, arg->msg_length, &is_string);
	return OK;
}

void WSLPeer::set_write_mode(WriteMode p_mode) {
	write_mode = p_mode;
}

WSLPeer::WriteMode WSLPeer::get_write_mode() const {
	return write_mode;
}

void WSLPeer::poll() {
	if (_wsl_context.is_null()) {
		return;
	}

	WebSocketPeer::State state = _wsl_context->get_state();
	if (state == STATE_CONNECTING) {
		_wsl_context->poll();
		state = _wsl_context->get_state();
	}

	// TODO Merge up
	if (state == STATE_OPEN || state == STATE_CLOSING) {
		_wsl_context->_wsl_poll();
		state = _wsl_context->get_state();
	}

	if (state == STATE_CLOSED) {
		_wsl_context.unref();
	}
}

Error WSLPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V(!is_connected_to_host(), FAILED);
	ERR_FAIL_COND_V(_out_pkt_size && (wslay_event_get_queued_msg_count(_wsl_context->get_ctx()) >= (1ULL << _out_pkt_size)), ERR_OUT_OF_MEMORY);
	ERR_FAIL_COND_V(_out_buf_size && (wslay_event_get_queued_msg_length(_wsl_context->get_ctx()) + p_buffer_size >= (1ULL << _out_buf_size)), ERR_OUT_OF_MEMORY);

	struct wslay_event_msg msg;
	msg.opcode = write_mode == WRITE_MODE_TEXT ? WSLAY_TEXT_FRAME : WSLAY_BINARY_FRAME;
	msg.msg = p_buffer;
	msg.msg_length = p_buffer_size;

	// Queue & send message.
	if (wslay_event_queue_msg(_wsl_context->get_ctx(), &msg) != 0 || wslay_event_send(_wsl_context->get_ctx()) != 0) {
		close_now();
		return FAILED;
	}
	return OK;
}

Error WSLPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {
	r_buffer_size = 0;

	ERR_FAIL_COND_V(!is_connected_to_host(), FAILED);

	if (_in_buffer.packets_left() == 0) {
		return ERR_UNAVAILABLE;
	}

	int read = 0;
	uint8_t *rw = _packet_buffer.ptrw();
	_in_buffer.read_packet(rw, _packet_buffer.size(), &_is_string, read);

	*r_buffer = rw;
	r_buffer_size = read;

	return OK;
}

int WSLPeer::get_available_packet_count() const {
	if (!is_connected_to_host()) {
		return 0;
	}

	return _in_buffer.packets_left();
}

int WSLPeer::get_current_outbound_buffered_amount() const {
	if (!is_connected_to_host()) {
		return 0;
	}

	return wslay_event_get_queued_msg_length(_wsl_context->get_ctx());
}

bool WSLPeer::was_string_packet() const {
	return _is_string;
}

bool WSLPeer::is_connected_to_host() const {
	return _wsl_context.is_valid() && _wsl_context->get_state() == STATE_OPEN;
}

void WSLPeer::close_now() {
	close(1000, "");
	_wsl_context.unref();
}

void WSLPeer::close(int p_code, String p_reason) {
	if (_wsl_context.is_valid()) {
		_wsl_context->close(p_code, p_reason);
	}

	_in_buffer.clear();
	_packet_buffer.resize(0);
}

IPAddress WSLPeer::get_connected_host() const {
	const Ref<StreamPeerTCP> tcp = _wsl_context.is_valid() ? _wsl_context->get_tcp() : Ref<StreamPeerTCP>();
	ERR_FAIL_COND_V(tcp.is_null(), IPAddress());

	return tcp->get_connected_host();
}

uint16_t WSLPeer::get_connected_port() const {
	const Ref<StreamPeerTCP> tcp = _wsl_context.is_valid() ? _wsl_context->get_tcp() : Ref<StreamPeerTCP>();
	ERR_FAIL_COND_V(tcp.is_null(), 0);

	return tcp->get_connected_port();
}

void WSLPeer::set_no_delay(bool p_enabled) {
	Ref<StreamPeerTCP> tcp = _wsl_context.is_valid() ? _wsl_context->get_tcp() : Ref<StreamPeerTCP>();
	ERR_FAIL_COND(tcp.is_null());

	tcp->set_no_delay(p_enabled);
}

WSLPeer::WSLPeer() {
}

WSLPeer::~WSLPeer() {
	close();
}

#endif // WEB_ENABLED
