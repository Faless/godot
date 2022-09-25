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
	WSLClientPeer *data = memnew(WSLClientPeer);
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
	WSLServerPeer *data = memnew(WSLServerPeer);
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
