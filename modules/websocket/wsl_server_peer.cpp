/*************************************************************************/
/*  wsl_server.cpp                                                       */
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

#include "wsl_server_peer.h"
#include "core/os/os.h"

bool WSLServerPeer::_parse_client_request(const Vector<String> p_protocols, String &r_resource_name) {
	Vector<String> psa = String((const char *)handshake_buffer->get_data_array().ptr()).split("\r\n");
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
	key = headers["sec-websocket-key"];
	if (headers.has("sec-websocket-protocol")) {
		Vector<String> protos = headers["sec-websocket-protocol"].split(",");
		for (int i = 0; i < protos.size(); i++) {
			String proto = protos[i].strip_edges();
			// Check if we have the given protocol
			for (int j = 0; j < p_protocols.size(); j++) {
				if (proto != p_protocols[j]) {
					continue;
				}
				protocol = proto;
				break;
			}
			// Found a protocol
			if (!protocol.is_empty()) {
				break;
			}
		}
		if (protocol.is_empty()) { // Invalid protocol(s) requested
			return false;
		}
	} else if (p_protocols.size() > 0) { // No protocol requested, but we need one
		return false;
	}
	return true;
}

Error WSLServerPeer::_do_server_handshake(const Vector<String> p_protocols, String &r_resource_name, const Vector<String> &p_extra_headers) {
	if (use_tls) {
		Ref<StreamPeerTLS> tls = static_cast<Ref<StreamPeerTLS>>(connection);
		if (tls.is_null()) {
			ERR_FAIL_V_MSG(ERR_BUG, "Couldn't get StreamPeerTLS for WebSocket handshake.");
			_state = STATE_CLOSED;
			return FAILED;
		}
		tls->poll();
		if (tls->get_status() == StreamPeerTLS::STATUS_HANDSHAKING) {
			return OK;
		} else if (tls->get_status() != StreamPeerTLS::STATUS_CONNECTED) {
			print_verbose(vformat("WebSocket SSL connection error during handshake (StreamPeerTLS status code %d).", tls->get_status()));
			_state = STATE_CLOSED;
			return FAILED;
		}
	}

	if (!has_request) {
		int read = 0;
		while (true) {
			ERR_FAIL_COND_V_MSG(handshake_buffer->get_available_bytes() < 1, ERR_OUT_OF_MEMORY, "WebSocket response headers are too big.");
			int pos = handshake_buffer->get_position();
			Vector<uint8_t> data = handshake_buffer->get_data_array();
			Error err = connection->get_partial_data(data.ptrw() + pos, 1, read);
			if (err != OK) { // Got an error
				print_verbose(vformat("WebSocket error while getting partial data (StreamPeer error code %d).", err));
				_state = STATE_CLOSED;
				return FAILED;
			} else if (read != 1) { // Busy, wait next poll
				return OK;
			}
			char *r = (char *)data.ptr();
			int l = pos;
			if (l > 3 && r[l] == '\n' && r[l - 1] == '\r' && r[l - 2] == '\n' && r[l - 3] == '\r') {
				r[l - 3] = '\0';
				if (!_parse_client_request(p_protocols, r_resource_name)) {
					_state = STATE_CLOSED;
					return FAILED;
				}
				String s = "HTTP/1.1 101 Switching Protocols\r\n";
				s += "Upgrade: websocket\r\n";
				s += "Connection: Upgrade\r\n";
				s += "Sec-WebSocket-Accept: " + WSLPeer::compute_key_response(key) + "\r\n";
				if (!protocol.is_empty()) {
					s += "Sec-WebSocket-Protocol: " + protocol + "\r\n";
				}
				for (int i = 0; i < p_extra_headers.size(); i++) {
					s += p_extra_headers[i] + "\r\n";
				}
				s += "\r\n";
				CharString cs = s.utf8();
				handshake_buffer->clear();
				handshake_buffer->put_data((const uint8_t *)cs.get_data(), cs.size());
				handshake_buffer->seek(0);
				has_request = true;
				break;
			}
			handshake_buffer->seek(pos + 1);
		}
	}

	if (!has_request) { // Still pending.
		return OK;
	}

	int left = handshake_buffer->get_available_bytes();
	if (has_request && left) {
		Vector<uint8_t> data = handshake_buffer->get_data_array();
		int pos = handshake_buffer->get_position();
		int sent = 0;
		Error err = connection->put_partial_data(data.ptr() + pos, left, sent);
		if (err != OK) {
			print_verbose(vformat("WebSocket error while putting partial data (StreamPeer error code %d).", err));
			_state = STATE_CLOSED;
			return err;
		}
		handshake_buffer->seek(pos + sent);
		left -= sent;
		if (left == 0) {
			_state = STATE_OPEN;
		}
	}

	return OK;
}

Error WSLServerPeer::poll() {
	if (_state == STATE_CONNECTING) {
		return _do_server_handshake(_protocols, resource_name, _extra_headers);
	}
	return OK;
}

WSLServerPeer::WSLServerPeer() {
}

WSLServerPeer::~WSLServerPeer() {
}

#endif // WEB_ENABLED
