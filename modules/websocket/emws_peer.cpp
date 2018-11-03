/*************************************************************************/
/*  emws_peer.cpp                                                        */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
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
#ifdef JAVASCRIPT_ENABLED

#include "emws_peer.h"
#include "core/io/ip.h"

void EMWSPeer::set_sock(int p_sock, unsigned int p_buffer_shift, unsigned int p_packets_shift) {

	peer_sock = p_sock;
	_in_buffer.set_payload_size(p_buffer_shift);
	_in_buffer.set_max_packets(p_packets_shift);
	_packet_buffer.resize((1 << p_buffer_shift) - 1);
}

void EMWSPeer::set_write_mode(WriteMode p_mode) {
	write_mode = p_mode;
}

EMWSPeer::WriteMode EMWSPeer::get_write_mode() const {
	return write_mode;
}

Error EMWSPeer::read_msg(uint8_t *p_data, uint32_t p_size, bool p_is_string) {

	if (_in_buffer.packets_space() < 1) {
		ERR_EXPLAIN("Too many packets in queue! Dropping data");
		ERR_FAIL_V(ERR_OUT_OF_MEMORY);
	}
	if (_in_buffer.payload_space() < p_size) {
		ERR_EXPLAIN("Buffer payload full! Dropping data");
		ERR_FAIL_V(ERR_OUT_OF_MEMORY);
	}

	PacketInfo info = {};
	info.is_string = p_is_string ? 1 : 0;
	info.size = p_size;
	_in_buffer.write_packet_info(&info);
	_in_buffer.write_packet_payload(p_data, p_size);
	return OK;
}

Error EMWSPeer::put_packet(const uint8_t *p_buffer, int p_buffer_size) {

	int is_bin = write_mode == WebSocketPeer::WRITE_MODE_BINARY ? 1 : 0;

	/* clang-format off */
	EM_ASM({
		var sock = Module.IDHandler.get($0);
		var bytes_array = new Uint8Array($2);
		var i = 0;

		for(i=0; i<$2; i++) {
			bytes_array[i] = getValue($1+i, 'i8');
		}

		if ($3) {
			sock.send(bytes_array.buffer);
		} else {
			var string = new TextDecoder("utf-8").decode(bytes_array);
			sock.send(string);
		}
	}, peer_sock, p_buffer, p_buffer_size, is_bin);
	/* clang-format on */

	return OK;
};

Error EMWSPeer::get_packet(const uint8_t **r_buffer, int &r_buffer_size) {

	if (_in_buffer.packets_left() == 0)
		return ERR_UNAVAILABLE;

	_in_buffer.read_packet_info(&_current_info);
	PoolVector<uint8_t>::Write rw = _packet_buffer.write();
	_in_buffer.read_packet_payload(rw.ptr(), _current_info.size);

	*r_buffer = rw.ptr();
	r_buffer_size = _current_info.size;

	return OK;
};

int EMWSPeer::get_available_packet_count() const {

	return _in_buffer.packets_left();
};

bool EMWSPeer::was_string_packet() const {

	return _current_info.is_string;
};

bool EMWSPeer::is_connected_to_host() const {

	return peer_sock != -1;
};

void EMWSPeer::close(int p_code, String p_reason) {

	if (peer_sock != -1) {
		/* clang-format off */
		EM_ASM({
			var sock = Module.IDHandler.get($0);
			var code = $1;
			var reason = UTF8ToString($2);
			sock.close(code, reason);
			Module.IDHandler.remove($0);
		}, peer_sock, p_code, p_reason.utf8().get_data());
		/* clang-format on */
	}
	memset(&_current_info, 0, sizeof(_current_info));
	_in_buffer.clear();
	peer_sock = -1;
};

IP_Address EMWSPeer::get_connected_host() const {

	ERR_EXPLAIN("Not supported in HTML5 export");
	ERR_FAIL_V(IP_Address());
};

uint16_t EMWSPeer::get_connected_port() const {

	ERR_EXPLAIN("Not supported in HTML5 export");
	ERR_FAIL_V(0);
};

EMWSPeer::EMWSPeer() {
	peer_sock = -1;
	write_mode = WRITE_MODE_BINARY;
	close();
};

EMWSPeer::~EMWSPeer() {

	close();
};

#endif // JAVASCRIPT_ENABLED
