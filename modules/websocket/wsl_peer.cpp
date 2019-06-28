/*************************************************************************/
/*  lws_peer.cpp                                                         */
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

#include "wsl_peer.h"

#include "wsl_client.h"
#include "wsl_helper.h"

#include "core/math/random_number_generator.h"
#include "core/os/os.h"

bool WSLPeer::_wsl_poll(struct PeerData *p_data) {
	p_data->polling = true;
	int err = 0;
	if ((err = wslay_event_recv(p_data->ctx)) != 0 || (err = wslay_event_send(p_data->ctx)) != 0) {
		WARN_PRINTS("ERROR! " + itos(err));
		p_data->destroy = true;
	}
	p_data->polling = false;
	if (p_data->destroy) {
		wslay_event_context_free(p_data->ctx);
		memdelete(p_data);
		return true;
	}
	return false;
}

ssize_t wsl_recv_callback(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {
	struct WSLPeer::PeerData *peer_data = (struct WSLPeer::PeerData *)user_data;
	if (peer_data->destroy) {
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	Ref<StreamPeer> conn = peer_data->conn;
	int read = 0;
	Error err = conn->get_partial_data(data, len, read);
	if (err != OK) {
		WARN_PRINTS(itos(err));
		wslay_event_set_error(ctx, WSLAY_ERR_CALLBACK_FAILURE);
		return -1;
	}
	if (read == 0) {
		wslay_event_set_error(ctx, WSLAY_ERR_WOULDBLOCK);
		return -1;
	}
	return read;
}

ssize_t wsl_send_callback(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
	struct WSLPeer::PeerData *peer_data = (struct WSLPeer::PeerData *)user_data;
	if (peer_data->destroy) {
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

int wsl_genmask_callback(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
	WARN_PRINTS("Genmask");
	RandomNumberGenerator rng;
	// TODO maybe use crypto in the future?
	rng.set_seed(OS::get_singleton()->get_unix_time());
	for (unsigned int i = 0; i < len; i++) {
		buf[i] = (uint8_t)rng.randi_range(0, 255);
	}
	return 0;
}

void wsl_msg_recv_callback(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
	WARN_PRINTS("Received message");
}

wslay_event_callbacks wsl_callbacks = {
	wsl_recv_callback,
	wsl_send_callback,
	wsl_genmask_callback,
	NULL, /* on_frame_recv_start_callback */
	NULL, /* on_frame_recv_callback */
	NULL, /* on_frame_recv_end_callback */
	wsl_msg_recv_callback
};

void WSLPeer::make_context(void *p_obj, Ref<StreamPeer> p_connection, unsigned int p_in_buf_size, unsigned int p_in_pkt_size, unsigned int p_out_buf_size, unsigned int p_out_pkt_size) {
	ERR_FAIL_COND(_data != NULL);

	_in_buffer.resize(p_in_pkt_size, p_in_buf_size);
	_out_buffer.resize(p_out_pkt_size, p_out_buf_size);
	_packet_buffer.resize((1 << MAX(p_in_buf_size, p_out_buf_size)) + LWS_PRE);
	_data = memnew(struct PeerData);
	_data->obj = p_obj;
	_data->conn = p_connection.ptr();
	wslay_event_context_client_init(&(_data->ctx), &wsl_callbacks, _data);
	_connection = p_connection;
}

void WSLPeer::set_write_mode(WriteMode p_mode) {
	write_mode = p_mode;
}

WSLPeer::WriteMode WSLPeer::get_write_mode() const {
	return write_mode;
}

void WSLPeer::poll() {
	if (!_data)
		return;

	if (_wsl_poll(_data)) {
		_data = NULL;
	}
}

Error WSLPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {

	ERR_FAIL_COND_V(!is_connected_to_host(), FAILED);

	struct wslay_event_msg msg; // Should I use fragmented?
	msg.opcode = write_mode == WRITE_MODE_TEXT ? WSLAY_TEXT_FRAME : WSLAY_BINARY_FRAME;
	msg.msg = p_buffer;
	msg.msg_length = p_buffer_size;

	wslay_event_queue_msg(_data->ctx, &msg);
	return OK;
};

Error WSLPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {

	r_buffer_size = 0;

	ERR_FAIL_COND_V(!is_connected_to_host(), FAILED);

	if (_in_buffer.packets_left() == 0)
		return ERR_UNAVAILABLE;

	int read = 0;
	PoolVector<uint8_t>::Write rw = _packet_buffer.write();
	_in_buffer.read_packet(rw.ptr(), _packet_buffer.size(), &_is_string, read);

	*r_buffer = rw.ptr();
	r_buffer_size = read;

	return OK;
};

int WSLPeer::get_available_packet_count() const {

	if (!is_connected_to_host())
		return 0;

	return _in_buffer.packets_left();
};

bool WSLPeer::was_string_packet() const {

	return _is_string;
};

bool WSLPeer::is_connected_to_host() const {

	return _data != NULL;
};

String WSLPeer::get_close_reason(void *in, size_t len, int &r_code) {
	String s;
	r_code = 0;
	if (len < 2) // From docs this should not happen
		return s;

	const uint8_t *b = (const uint8_t *)in;
	r_code = b[0] << 8 | b[1];

	if (len > 2) {
		const char *utf8 = (const char *)&b[2];
		s.parse_utf8(utf8, len - 2);
	}
	return s;
}

void WSLPeer::send_close_status() {
	if (close_code == -1)
		return;

	int len = close_reason.size();
	ERR_FAIL_COND(len > 123); // Maximum allowed reason size in bytes

	//lws_close_status code = (lws_close_status)close_code;
	//unsigned char *reason = len > 0 ? (unsigned char *)close_reason.utf8().ptrw() : NULL;

	//lws_close_reason(p_wsi, code, reason, len);

	close_code = -1;
	close_reason = "";
}

void WSLPeer::close(int p_code, String p_reason) {
	if (_data) {
		if (_data->polling)
			_data->destroy = true;
		else {
			wslay_event_context_free(_data->ctx);
			memdelete(_data);
		}
		_data = NULL;
	}
	if (false) {
		close_code = p_code;
		close_reason = p_reason;
	} else {
		close_code = -1;
		close_reason = "";
	}
	_in_buffer.clear();
	_out_buffer.clear();
	_in_size = 0;
	_is_string = 0;
	_packet_buffer.resize(0);
};

IP_Address WSLPeer::get_connected_host() const {

	ERR_FAIL_COND_V(!is_connected_to_host(), IP_Address());

	IP_Address ip;
	return ip;
};

uint16_t WSLPeer::get_connected_port() const {

	ERR_FAIL_COND_V(!is_connected_to_host(), 0);

	uint16_t port = 0;
	return port;
};

WSLPeer::WSLPeer() {
	_data = NULL;
	write_mode = WRITE_MODE_BINARY;
	close();
};

WSLPeer::~WSLPeer() {

	close();
};

#endif // JAVASCRIPT_ENABLED
