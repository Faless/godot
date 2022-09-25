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
#include "wsl_server.h"

#include "core/crypto/crypto_core.h"
#include "core/math/random_number_generator.h"
#include "core/os/os.h"

Error WSLPeer::connect_to_url(String p_url, const Vector<String> p_protocols = Vector<String>(), const Vector<String> p_custom_headers = Vector<String>(), bool p_verify_tls = true, Ref<X509Certificate> p_cert = Ref<X509Certificate>()) {
	ERR_FAIL_COND_V(_connection.is_valid(), ERR_ALREADY_IN_USE);
	String host = p_url;
	String path;
	String scheme;
	int port = 0;
	Error err = p_url.parse_url(scheme, host, port, path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Invalid URL: " + p_url);

	_host = host;
	_port = port;
	_use_tls = false;
	if (scheme == "wss://") {
		_use_tls = true;
	}
	if (port == 0) {
		port = _use_tls ? 443 : 80;
	}
	if (path.is_empty()) {
		path = "/";
	}

	_peer = Ref<WSLPeer>(memnew(WSLPeer));

	if (_host.is_valid_ip_address()) {
		_ip_candidates.push_back(IPAddress(_host));
	} else {
		// Queue hostname for resolution.
		_resolver_id = IP::get_singleton()->resolve_hostname_queue_item(_host);
		ERR_FAIL_COND_V(_resolver_id == IP::RESOLVER_INVALID_ID, ERR_INVALID_PARAMETER);
		// Check if it was found in cache.
		IP::ResolverStatus ip_status = IP::get_singleton()->get_resolve_item_status(_resolver_id);
		if (ip_status == IP::RESOLVER_STATUS_DONE) {
			_ip_candidates = IP::get_singleton()->get_resolve_item_addresses(_resolver_id);
			IP::get_singleton()->erase_resolve_item(_resolver_id);
			_resolver_id = IP::RESOLVER_INVALID_ID;
		}
	}

	// We assume OK while hostname resolution is pending.
	Error err = _resolver_id != IP::RESOLVER_INVALID_ID ? OK : FAILED;
	while (_ip_candidates.size()) {
		err = _tcp->connect_to_host(_ip_candidates.pop_front(), _port);
		if (err == OK) {
			break;
		}
	}
	if (err != OK) {
		_tcp->disconnect_from_host();
		return err;
	}
	_connection = _tcp;
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
	_handshake_buffer.put_data((const uint8_t *)cs.get_data(), cs.size());
	_status = STATUS_CONNECTING;
	_is_client = true;

	return OK;
}

void WSLPeer::_client_poll() {
	if (_resolver_id != IP::RESOLVER_INVALID_ID) {
		IP::ResolverStatus ip_status = IP::get_singleton()->get_resolve_item_status(_resolver_id);
		if (ip_status == IP::RESOLVER_STATUS_WAITING) {
			return;
		}
		// Anything else is either a candidate or a failure.
		Error err = FAILED;
		if (ip_status == IP::RESOLVER_STATUS_DONE) {
			_ip_candidates = IP::get_singleton()->get_resolve_item_addresses(_resolver_id);
			while (_ip_candidates.size()) {
				err = _tcp->connect_to_host(_ip_candidates.pop_front(), _port);
				if (err == OK) {
					break;
				}
			}
		}
		IP::get_singleton()->erase_resolve_item(_resolver_id);
		_resolver_id = IP::RESOLVER_INVALID_ID;
		if (err != OK) {
			_status = STATUS_CLOSED;
			_clear();
			return;
		}
	}
	if (_peer->is_connected_to_host()) {
		_peer->poll();
		if (!_peer->is_connected_to_host()) {
			_status = STATUS_CLOSED;
			_clear();
		}
		return;
	}

	if (_connection.is_null()) {
		return; // Not connected.
	}

	_tcp->poll();
	switch (_tcp->get_status()) {
		case StreamPeerTCP::STATUS_NONE:
			// Clean close
			_status = STATUS_CLOSED;
			_clear();
			break;
		case StreamPeerTCP::STATUS_CONNECTED: {
			_ip_candidates.clear();
			Ref<StreamPeerTLS> tls;
			if (_use_tls) {
				if (_connection == _tcp) {
					// Start SSL handshake
					tls = Ref<StreamPeerTLS>(StreamPeerTLS::create());
					ERR_FAIL_COND_MSG(tls.is_null(), "SSL is not available in this build.");
					tls->set_blocking_handshake_enabled(false);
					if (tls->connect_to_stream(_tcp, verify_tls, _host, tls_cert) != OK) {
						disconnect_from_host();
						_on_error();
						return;
					}
					_connection = tls;
				} else {
					tls = static_cast<Ref<StreamPeerTLS>>(_connection);
					ERR_FAIL_COND(tls.is_null()); // Bug?
					tls->poll();
				}
				if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
					return; // Need more polling.
				} else if (tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
					disconnect_from_host();
					_on_error();
					return; // Error.
				}
			}
			// Do websocket handshake.
			_do_handshake();
		} break;
		case StreamPeerTCP::STATUS_ERROR:
			while (_ip_candidates.size() > 0) {
				_tcp->disconnect_from_host();
				if (_tcp->connect_to_host(_ip_candidates.pop_front(), _port) == OK) {
					return;
				}
			}
			disconnect_from_host();
			_on_error();
			break;
		case StreamPeerTCP::STATUS_CONNECTING:
			break; // Wait for connection
	}
}

void WSLPeer::_clear() {
	// TODO more, from close.
	_connection.unref();
	_tcp.unref();
	_tcp.instantiate();
	_state = STATE_CLOSED;

	_key = "";
	_host = "";
	_protocols.clear();
	_use_tls = false;

	_handshake_buffer->clear();

	if (_resolver_id != IP::RESOLVER_INVALID_ID) {
		IP::get_singleton()->erase_resolve_item(_resolver_id);
		_resolver_id = IP::RESOLVER_INVALID_ID;
	}

	_ip_candidates.clear();
}

void WSLPeer::_do_client_handshake() {
	if (_pending_request) {
		int left = _handshake_buffer->get_available_bytes();
		int pos = _handshake_buffer->get_position();
		const Vector<uint8_t> data = _handshake_buffer->get_data_array();
		int sent = 0;
		Error err = _connection->put_partial_data(data.ptr() + pos, left, sent);
		// Sending handshake failed
		if (err != OK) {
			// TODO err
			_state = STATE_CLOSED;
			_clear();
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
				_state = STATE_CLOSED;
				_clear();
				ERR_FAIL_MSG("Response headers too big.");
				return;
			}

			Error err = _connection->get_partial_data(data.ptrw() + pos, 1, read);
			if (err == ERR_FILE_EOF) {
				// We got a disconnect.
				_state = STATE_CLOSED;
				_clear();
				return;
			} else if (err != OK) {
				// Got some error.
				_state = STATE_CLOSED;
				_clear();
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
					_state = STATE_CLOSED;
					_clear();
					ERR_FAIL_MSG("Invalid response headers.");
				}
				// Create peer.
				WSLPeer::PeerData *data = memnew(struct WSLPeer::PeerData);
				data->obj = this;
				data->conn = _connection;
				data->tcp = _tcp;
				data->is_server = false;
				data->id = 1;
				make_context(data, _in_buf_size, _in_pkt_size, _out_buf_size, _out_pkt_size);
				set_no_delay(true);
				_state = STATE_OPEN;
				_handshaking = false;
				break;
			}
		}
	}
}

bool WSLPeer::_verify_server_response(String &r_protocol) {
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

void WSLPeer::_wsl_destroy(struct PeerData **p_data) {
	if (!p_data || !(*p_data)) {
		return;
	}
	struct PeerData *data = *p_data;
	if (data->polling) {
		data->destroy = true;
		return;
	}
	wslay_event_context_free(data->ctx);
	memdelete(data);
	*p_data = nullptr;
}

bool WSLPeer::_wsl_poll(struct PeerData *p_data) {
	p_data->polling = true;
	int err = 0;
	if ((err = wslay_event_recv(p_data->ctx)) != 0 || (err = wslay_event_send(p_data->ctx)) != 0) {
		print_verbose("Websocket (wslay) poll error: " + itos(err));
		p_data->destroy = true;
	}
	p_data->polling = false;

	if (p_data->destroy || (wslay_event_get_close_sent(p_data->ctx) && wslay_event_get_close_received(p_data->ctx))) {
		bool valid = p_data->valid;
		_wsl_destroy(&p_data);
		return valid;
	}
	return false;
}

ssize_t WSLPeer::_wsl_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {
	struct WSLPeer::PeerData *peer_data = (struct WSLPeer::PeerData *)user_data;
	if (!peer_data->valid) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	Ref<StreamPeer> conn = peer_data->conn;
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

ssize_t WSLPeer::_wsl_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
	struct WSLPeer::PeerData *peer_data = (struct WSLPeer::PeerData *)user_data;
	if (!peer_data->valid) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	Ref<StreamPeer> conn = peer_data->conn;
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

int WSLPeer::_wsl_genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
	RandomNumberGenerator rng;
	// TODO maybe use crypto in the future?
	rng.set_seed(OS::get_singleton()->get_unix_time());
	for (unsigned int i = 0; i < len; i++) {
		buf[i] = (uint8_t)rng.randi_range(0, 255);
	}
	return 0;
}

void WSLPeer::_wsl_msg_recv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
	struct WSLPeer::PeerData *peer_data = (struct WSLPeer::PeerData *)user_data;
	if (!peer_data->valid || peer_data->closing) {
		return;
	}
	WSLPeer *peer = static_cast<WSLPeer *>(peer_data->peer);

	if (peer->parse_message(arg) != OK) {
		return;
	}

	if (peer_data->is_server) {
		WSLServer *helper = static_cast<WSLServer *>(peer_data->obj);
		helper->_on_peer_packet(peer_data->id);
	} else {
		WSLClient *helper = static_cast<WSLClient *>(peer_data->obj);
		helper->_on_peer_packet();
	}
}

wslay_event_callbacks WSLPeer::_wsl_callbacks = {
	_wsl_recv_callback,
	_wsl_send_callback,
	_wsl_genmask_callback,
	nullptr, /* on_frame_recv_start_callback */
	nullptr, /* on_frame_recv_callback */
	nullptr, /* on_frame_recv_end_callback */
	_wsl_msg_recv_callback
};

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
		if (!wslay_event_get_close_sent(_data->ctx)) {
			if (_data->is_server) {
				WSLServer *helper = static_cast<WSLServer *>(_data->obj);
				helper->_on_close_request(_data->id, close_code, close_reason);
			} else {
				WSLClient *helper = static_cast<WSLClient *>(_data->obj);
				helper->_on_close_request(close_code, close_reason);
			}
		}
		return ERR_FILE_EOF;
	} else if (arg->opcode != WSLAY_BINARY_FRAME) {
		// Ping or pong
		return ERR_SKIP;
	}
	_in_buffer.write_packet(arg->msg, arg->msg_length, &is_string);
	return OK;
}

void WSLPeer::make_context(PeerData *p_data, unsigned int p_in_buf_size, unsigned int p_in_pkt_size, unsigned int p_out_buf_size, unsigned int p_out_pkt_size) {
	ERR_FAIL_COND(_data != nullptr);
	ERR_FAIL_COND(p_data == nullptr);

	_in_buffer.resize(p_in_pkt_size, p_in_buf_size);
	_packet_buffer.resize(1 << p_in_buf_size);
	_out_buf_size = p_out_buf_size;
	_out_pkt_size = p_out_pkt_size;

	_data = p_data;
	_data->peer = this;
	_data->valid = true;

	if (_data->is_server) {
		wslay_event_context_server_init(&(_data->ctx), &_wsl_callbacks, _data);
	} else {
		wslay_event_context_client_init(&(_data->ctx), &_wsl_callbacks, _data);
	}
	wslay_event_config_set_max_recv_msg_length(_data->ctx, (1ULL << p_in_buf_size));
}

void WSLPeer::set_write_mode(WriteMode p_mode) {
	write_mode = p_mode;
}

WSLPeer::WriteMode WSLPeer::get_write_mode() const {
	return write_mode;
}

void WSLPeer::poll() {
	if (_handshaking && _is_client) { // TODO also for server.
		return _do_client_handshake();
	}

	if (!_data) {
		return;
	}

	if (_wsl_poll(_data)) {
		_data = nullptr;
	}
}

Error WSLPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {
	ERR_FAIL_COND_V(!is_connected_to_host(), FAILED);
	ERR_FAIL_COND_V(_out_pkt_size && (wslay_event_get_queued_msg_count(_data->ctx) >= (1ULL << _out_pkt_size)), ERR_OUT_OF_MEMORY);
	ERR_FAIL_COND_V(_out_buf_size && (wslay_event_get_queued_msg_length(_data->ctx) + p_buffer_size >= (1ULL << _out_buf_size)), ERR_OUT_OF_MEMORY);

	struct wslay_event_msg msg;
	msg.opcode = write_mode == WRITE_MODE_TEXT ? WSLAY_TEXT_FRAME : WSLAY_BINARY_FRAME;
	msg.msg = p_buffer;
	msg.msg_length = p_buffer_size;

	// Queue & send message.
	if (wslay_event_queue_msg(_data->ctx, &msg) != 0 || wslay_event_send(_data->ctx) != 0) {
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
	ERR_FAIL_COND_V(!_data, 0);

	return wslay_event_get_queued_msg_length(_data->ctx);
}

bool WSLPeer::was_string_packet() const {
	return _is_string;
}

bool WSLPeer::is_connected_to_host() const {
	return _data != nullptr;
}

void WSLPeer::close_now() {
	close(1000, "");
	_wsl_destroy(&_data);
}

void WSLPeer::close(int p_code, String p_reason) {
	if (_data && !wslay_event_get_close_sent(_data->ctx)) {
		CharString cs = p_reason.utf8();
		wslay_event_queue_close(_data->ctx, p_code, (uint8_t *)cs.ptr(), cs.size());
		wslay_event_send(_data->ctx);
		_data->closing = true;
	}

	_in_buffer.clear();
	_packet_buffer.resize(0);
}

IPAddress WSLPeer::get_connected_host() const {
	ERR_FAIL_COND_V(!is_connected_to_host() || _data->tcp.is_null(), IPAddress());

	return _data->tcp->get_connected_host();
}

uint16_t WSLPeer::get_connected_port() const {
	ERR_FAIL_COND_V(!is_connected_to_host() || _data->tcp.is_null(), 0);

	return _data->tcp->get_connected_port();
}

void WSLPeer::set_no_delay(bool p_enabled) {
	ERR_FAIL_COND(!is_connected_to_host() || _data->tcp.is_null());
	_data->tcp->set_no_delay(p_enabled);
}

void WSLPeer::invalidate() {
	if (_data) {
		_data->valid = false;
	}
}

WSLPeer::WSLPeer() {
}

WSLPeer::~WSLPeer() {
	close();
	invalidate();
	_wsl_destroy(&_data);
	_data = nullptr;
}

#endif // WEB_ENABLED
